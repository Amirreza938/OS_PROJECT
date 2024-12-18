#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "data_load.h"  // Ensure this header file includes the struct definitions
#include <sys/mman.h>  // For shm_open and mmap
#include <fcntl.h>   
#include <semaphore.h>

struct process_params {
    struct product* prod;
    char* storeName;
    char* categoryName;
    struct buyBox buyBox;
    int pipe_fd;
};

void get_current_time(char* buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

void decrease_entity_count(const char* store_name, const char* category_name, int product_id, int quantity) {
    // Construct the file path
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/%s/%d.txt", store_name, category_name, product_id);

    // Open the file for reading and writing
    FILE* file = fopen(file_path, "r+");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    struct product prod;
    char line[256];
    
    // Read product details from the file
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "Name: %[^\n]", prod.Name) == 1) continue;
        if (sscanf(line, "Price: %f", &prod.Price) == 1) continue;
        if (sscanf(line, "Score: %f", &prod.Score) == 1) continue;
        if (sscanf(line, "Entity: %d", &prod.Entity) == 1) continue;
        if (sscanf(line, "Last Modified: %[^\n]", prod.LastModified) == 1) continue;
    }

    // Check if there are enough entities to fulfill the order
    if (prod.Entity < quantity) {
        printf("Not enough entities available. Current: %d, Requested: %d\n", prod.Entity, quantity);
        fclose(file);
        return;
    }

    // Decrease the entity count
    prod.Entity -= quantity;

    get_current_time(prod.LastModified, sizeof(prod.LastModified));

    // Move the file pointer to the beginning of the file to overwrite it
    rewind(file);

    // Write updated product details back to the file
    fprintf(file, "Name: %s\n", prod.Name);
    fprintf(file, "Price: %.2f\n", prod.Price);
    fprintf(file, "Score: %.1f\n", prod.Score);
    fprintf(file, "Entity: %d\n", prod.Entity);
    fprintf(file, "Last Modified: %s\n", prod.LastModified);

    printf("files updated");
    // Close the file
    fclose(file);
}


// Function to process a single product
void* process_product(void* arg) {
    // Unpack parameters
    struct process_params* params = (struct process_params*)arg;
    struct product* prod = params->prod;
    int pipe_fd = params->pipe_fd;


    // Free the allocated parameter array
    free(params);

    for (int i = 0; i < params->buyBox.OrderCount; i++) {
        if (strcmp(prod->Name, params->buyBox.Orders[i].product_name) == 0) {
            if (prod->Entity >= params->buyBox.Orders[i].quantity) {
                // Write product details to the pipe
                write(pipe_fd, prod, sizeof(struct product));
                break;
            }
        }
    }

    return NULL;
}

void process_category(struct category* cat, struct buyBox UserBuyBox, struct sellBox* StoreSellBox, int store_pid, int pipe_fd,char StoreName[256]) {
    printf("PID %d create child for category: %s PID: %d\n", store_pid, cat->Name, getpid());

    pthread_t* threads = malloc(cat->ProductCount * sizeof(pthread_t));
    if (!threads) {
        perror("Failed to allocate memory for threads");
        exit(1);
    }

    // Create threads for each product
    for (int i = 0; i < cat->ProductCount; i++) {
        // Allocate memory for parameter array
        struct process_params* params = malloc(sizeof(struct process_params));
        if (!params) {
            perror("Failed to allocate memory for thread parameters");
            free(threads);
            exit(EXIT_FAILURE);
        }

        params->prod = &cat->Products[i];   // Pass pointer to the product
        params->storeName = StoreName;       // Pass store name
        params->categoryName = cat->Name;     // Pass category name
        params->pipe_fd = pipe_fd;            // Pass pipe file descriptor
        params->buyBox = UserBuyBox;

        if (pthread_create(&threads[i], NULL, process_product, (void*)params) != 0) {
            perror("Failed to create thread for product");
            free(threads);
            free(params);
            exit(1);
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < cat->ProductCount; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
}

// Function to handle a store
void process_store(struct store *s, struct buyBox UserBuyBox, struct sellBox *StoreSellBox, int parent_pid, int pipe_fd) {
    printf("PID %d created child for store: %s\n", parent_pid, s->Name);

    for (int i = 0; i < s->CategoryCount; i++) {
        pid_t pid = fork();
        if (pid == 0) {  // Child process for categories
            process_category(&s->Categories[i], UserBuyBox, StoreSellBox, getpid(), pipe_fd,s->Name);
            exit(0);
        }
        else if(pid > 0)
        {
           wait(NULL); 
        } else {
            perror("fork failed");
            exit(1);
        }
    }
}

float calculateScore(struct sellBox* box) {
    float totalScore = 0.0;
    for (int i = 0; i < box->ProductCount; i++) {
        totalScore += box->products[i].Score * box->products[i].Price;
    }
    return totalScore;
}
// Function to find the best sellBox dynamically
struct sellBox getBestBox(int pipe_fd[][2], int store_count) {
    struct sellBox userSellBox;
    userSellBox.products = NULL;
    userSellBox.ProductCount = 0;

    float bestScore = -1.0;

    // Iterate through each pipe to read sellBoxes
    for (int i = 0; i < store_count; i++) {
        close(pipe_fd[i][1]); // Close the write end in the parent

        // Read product data dynamically into a sellBox
        struct sellBox currentBox;
        currentBox.ProductCount = 0;
        currentBox.products = malloc(0); // Start with no products

        struct product prod;
        while (read(pipe_fd[i][0], &prod, sizeof(struct product)) > 0) {
            
            currentBox.ProductCount++;
            currentBox.products = realloc(currentBox.products, currentBox.ProductCount * sizeof(struct product));
            if (!currentBox.products) {
                perror("Memory allocation failed");
                exit(EXIT_FAILURE);
            }
            currentBox.products[currentBox.ProductCount - 1] = prod;
        }
        
        close(pipe_fd[i][0]); // Close the read end after reading

        // Calculate the score for this sellBox
        float currentScore = calculateScore(&currentBox);
        if (currentScore > bestScore) {
            // Free the previous best sellBox's memory
            if (userSellBox.products != NULL) {
                free(userSellBox.products);
            }

            // Update the best sellBox
            bestScore = currentScore;
            userSellBox = currentBox;
        } else {
            // Free the memory for the current box if it's not the best
            free(currentBox.products);
        }
    }

    return userSellBox;
}

// void createSharedMemory(){
//     const char * name = "shared_data";
//     int shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
//     if (shm_fd == -1) {
//         perror("shm_open");
//         exit(EXIT_FAILURE);
//     }
//     // Configure the size of the shared memory
//     size_t size = sizeof(struct shared_data);
//     if (ftruncate(shm_fd, size) == -1) {
//         perror("ftruncate");
//         exit(EXIT_FAILURE);
//     }

//      // Map the shared memory object in the address space
//     sh_data = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
//     if (sh_data == MAP_FAILED) {
//         perror("mmap");
//         exit(EXIT_FAILURE);
//     }

//     // Initialize mutex and semaphore
//     pthread_mutexattr_t attr;
//     pthread_mutexattr_init(&attr);
//     pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
//     pthread_mutex_init(&sh_data->mutex, &attr);
//     sem_init(&sh_data->sem, 1, 0); // Initialize semaphore for inter-process use

//     // Initialize other fields
//     sh_data->awake_signal = 0;
//     sh_data->purchase_signal = 0;
// }

int main() {


    char username[50];
    int order_count;

    // Input: Username
    printf("Username: ");
    scanf("%49s", username);

    // Input: Number of Orders
    printf("Enter number of orders: ");
    scanf("%d", &order_count);

    struct order* orders = malloc(order_count * sizeof(struct order));
    if (!orders) {
        perror("Failed to allocate memory for orders");
        exit(1);
    }

    printf("Enter OrderList (product_name quantity):\n");
    for (int i = 0; i < order_count; i++) {
        scanf("%s %d", orders[i].product_name, &orders[i].quantity);
    }

    float price_threshold;
    printf("Price threshold: ");
    scanf("%f", &price_threshold);

    printf("\n%s created PID: %d\n", username, getpid());

    struct store *stores = NULL;
    int store_count = 0;
    get_stores("./Data_set", &stores, &store_count);

    struct buyBox data = {orders, order_count};

    // Create a pipe for each store
    int pipe_fd[store_count][2];
    for (int i = 0; i < store_count; i++) {
        if (pipe(pipe_fd[i]) == -1) {
            perror("pipe failed");
            free(orders);
            exit(1);
        }
    }

    for (int i = 0; i < store_count; i++) {
        pid_t pid = fork();

        struct sellBox* StoreSellBox_ptr = malloc(sizeof(struct sellBox));
        if (!StoreSellBox_ptr) {
            perror("Failed to allocate memory for StoreSellBox");
            free(orders);
            exit(1);
        }
        StoreSellBox_ptr->products = NULL; // Ensure products pointer is initialized
        StoreSellBox_ptr->ProductCount = 0;

        if (pid == 0) {  // Child process for store
            process_store(&stores[i], data, StoreSellBox_ptr, getppid(), pipe_fd[i][1]);
            // Child does not print products, it writes them to the pipe
            free(StoreSellBox_ptr->products);
            free(StoreSellBox_ptr);
            exit(0);  // Exit after processing the store
        }
        else if (pid < 0) {
            perror("fork failed");
            free(StoreSellBox_ptr);
            free(orders);
            exit(1);
        }
    }   

    for (int i = 0; i < store_count; i++) {
        wait(NULL);
    }
    
    struct sellBox box = getBestBox(pipe_fd,store_count);
    printf("best store:\n");
    for (int i=0; i < box.ProductCount ; i++){
        printf("product name: %s and product price: %f and product score : %f\n",box.products[i].Name,box.products[i].Price , box.products[i].Score); 
    }

    free(orders);
    free(stores);

    return 0;
}
