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
#include <time.h>
struct process_params {
    struct product* prod;
    char* storeName;
    char* categoryName;
    struct buyBox buyBox;
    int pipe_fd;
    char* username;
};

void get_current_time(char* buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}
// Mutex for thread-safe logging
pthread_mutex_t log_mutex;

void write_log(const char* username, const char* message) {
    // Create the log file name based on the username
    char log_filename[100];
    snprintf(log_filename, sizeof(log_filename), "%s_Order0.log", username);

    // Mutex for thread-safe logging
    pthread_mutex_lock(&log_mutex); // Lock the mutex for thread safety

    FILE* log_file = fopen(log_filename, "a");
    if (log_file != NULL) {
        // Write log message with timestamp
        char time_buffer[64];
        get_current_time(time_buffer, sizeof(time_buffer));
        fprintf(log_file, "[%s] %s\n", time_buffer, message);
        fclose(log_file);
    } else {
        perror("Error opening log file");
    }
    
    pthread_mutex_unlock(&log_mutex); // Unlock the mutex
}


// Initialize logging system
void init_logging() {
    pthread_mutex_init(&log_mutex, NULL); // Initialize the mutex
    FILE* log_file = fopen("logfile.txt", "w"); // Clear log file at program start
    if (log_file != NULL) {
        fclose(log_file);
    } else {
        perror("Error initializing log file");
    }
}

// Clean up logging system
void cleanup_logging() {
    pthread_mutex_destroy(&log_mutex); // Destroy the mutex
}
void decrease_entity_count(const char* store_name, const char* category_name, int product_id, int quantity) {
    // Construct the file path
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "./Data_set/%s/%s/%d.txt", store_name, category_name, product_id);

    // Open the file for reading and writing
    FILE* file = fopen(file_path, "r+");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    struct product prod = {0};
    char line[256];
    int entity_found = 0;

    // Read product details
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "Name: %[^\n]", prod.Name) == 1) continue;
        if (sscanf(line, "Price: %f", &prod.Price) == 1) continue;
        if (sscanf(line, "Score: %f", &prod.Score) == 1) continue;
        if (sscanf(line, "Entity: %d", &prod.Entity) == 1) {
            entity_found = 1;
            continue;
        }
        if (sscanf(line, "Last Modified: %[^\n]", prod.LastModified) == 1) continue;
    }

    // Ensure "Entity" field is found in the file
    if (!entity_found) {
        printf("Entity count not found in the file.\n");
        fclose(file);
        return;
    }

    // Check if enough entities are available
    if (prod.Entity < quantity) {
        printf("Not enough entities available. Current: %d, Requested: %d\n", prod.Entity, quantity);
        fclose(file);
        return;
    }

    // Update the entity count and last modified time
    prod.Entity -= quantity;
    get_current_time(prod.LastModified, sizeof(prod.LastModified));

    // Move the file pointer to the beginning of the file for overwriting
    rewind(file);

    // Write updated product details
    fprintf(file, "Name: %s\n", prod.Name);
    fprintf(file, "Price: %.2f\n", prod.Price);
    fprintf(file, "Score: %.1f\n", prod.Score);
    fprintf(file, "Entity: %d\n", prod.Entity);
    fprintf(file, "Last Modified: %s\n", prod.LastModified);

    printf("Product updated successfully. Remaining Entity: %d\n", prod.Entity);

    fclose(file);
}
// Function to process a single product
void* process_product(void* arg) {
    // Unpack parameters
    struct process_params* params = (struct process_params*)arg;
    struct product* prod = params->prod;
    int pipe_fd = params->pipe_fd;
    const char* username = params->username;
    // Log thread start and file being processed
    char log_message[512];
    snprintf(log_message, sizeof(log_message), "Thread started for file: ./Data_set/%s/%s/%d.txt",
             params->storeName, params->categoryName, prod->id);
    write_log(username, log_message);

    for (int i = 0; i < params->buyBox.OrderCount; i++) {
        if (strcmp(prod->Name, params->buyBox.Orders[i].product_name) == 0) {
            if (prod->Entity >= params->buyBox.Orders[i].quantity) {
                // Log product found
                snprintf(log_message, sizeof(log_message), "Thread found product: %s in store: %s, category: %s",
                         prod->Name, params->storeName, params->categoryName);
                write_log(username,log_message);

                // Write product details to the pipe
                write(pipe_fd, prod, sizeof(struct product));
                break;
            }
        }
    }

    // Free the allocated parameter array
    free(params);
    return NULL;
}
float calculateScore(struct sellBox* box) {
    float totalScore = 0.0;
    for (int i = 0; i < box->ProductCount; i++) {
        totalScore += box->products[i].Score * box->products[i].Price;
    }
    return totalScore;
}
// Function to find the best sellBox dynamically
/*struct sellBox getBestBox(int pipe_fd[][2], int store_count) {
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
}*/
struct sellBox getBestBox(int pipe_fd[][2], int store_count, float price_threshold) {
    struct sellBox userSellBox;
    userSellBox.products = NULL;
    userSellBox.ProductCount = 0;

    struct sellBox* allStoreBoxes = malloc(store_count * sizeof(struct sellBox));
    float* totalPrices = malloc(store_count * sizeof(float));
    int validStoreCount = 0;

    // Read data from all stores
    for (int i = 0; i < store_count; i++) {
        close(pipe_fd[i][1]); // Close write end

        allStoreBoxes[i].ProductCount = 0;
        allStoreBoxes[i].products = malloc(0);
        totalPrices[i] = 0.0;

        struct product prod;
        while (read(pipe_fd[i][0], &prod, sizeof(struct product)) > 0) {
            allStoreBoxes[i].ProductCount++;
            allStoreBoxes[i].products = realloc(allStoreBoxes[i].products, 
                allStoreBoxes[i].ProductCount * sizeof(struct product));
            
            allStoreBoxes[i].products[allStoreBoxes[i].ProductCount - 1] = prod;
            totalPrices[i] += prod.Price;
        }
        
        close(pipe_fd[i][0]);

        // Check if this store has all products and is within price threshold
        if (totalPrices[i] <= price_threshold || price_threshold == -1) {
            validStoreCount++;
        }
    }

    // Find the best store among valid ones
    float bestScore = -1.0;
    int bestStoreIndex = -1;

    for (int i = 0; i < store_count; i++) {
        if (totalPrices[i] <= price_threshold || price_threshold == -1) {
            float currentScore = calculateScore(&allStoreBoxes[i]);
            if (currentScore > bestScore) {
                bestScore = currentScore;
                bestStoreIndex = i;
            }
        }
    }

    // Copy the best store's data
    if (bestStoreIndex != -1) {
        userSellBox = allStoreBoxes[bestStoreIndex];
    }

    // Free memory for other stores
    for (int i = 0; i < store_count; i++) {
        if (i != bestStoreIndex) {
            free(allStoreBoxes[i].products);
        }
    }

    free(allStoreBoxes);
    free(totalPrices);

    return userSellBox;
}
// Function to process a category
void process_category(struct category* cat, struct buyBox UserBuyBox, struct sellBox* StoreSellBox, int store_pid, int pipe_fd, char StoreName[256],const char* username) {
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
        params->username = username;

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
void process_store(struct store *s, struct buyBox UserBuyBox, struct sellBox *StoreSellBox, int parent_pid, int pipe_fd,const char* username) {
    printf("PID %d created child for store: %s\n", parent_pid, s->Name);

    for (int i = 0; i < s->CategoryCount; i++) {
        pid_t pid = fork();
        if (pid == 0) {  // Child process for categories
            process_category(&s->Categories[i], UserBuyBox, StoreSellBox, getpid(), pipe_fd, s->Name,username);
            exit(0);
        }
        else if(pid > 0) {
           wait(NULL); 
        } else {
            perror("fork failed");
            exit(1);
        }
    }
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
    // Initialize logging
    init_logging();

    char username[50];
    char ans[20];
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
    printf("Price threshold (if it is not important for you enter -1): ");
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
            process_store(&stores[i], data, StoreSellBox_ptr, getppid(), pipe_fd[i][1],username);
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
    /*struct sellBox best_store = getBestBox(pipe_fd, store_count);
    printf("Best store products:\n");

    for (int i = 0; i < best_store.ProductCount; i++) {
        struct product* prod = &best_store.products[i];
        printf(
            "Product: %s, Price: %.2f, Score: %.1f, Store: %s\n",
            prod->Name,
            prod->Price,
            prod->Score,
            prod->StoreName
        );

        // Find the matching order to get the correct quantity
        for (int j = 0; j < data.OrderCount; j++) {
            if (strcmp(prod->Name, data.Orders[j].product_name) == 0) {
                int requested_quantity = data.Orders[j].quantity;

                // Decrease the correct quantity
                decrease_entity_count(prod->StoreName, prod->CategoryName, prod->id, requested_quantity);
                break;
            }
        }
    }*/
    // In main()
    struct sellBox best_store = getBestBox(pipe_fd, store_count, price_threshold);

    if (best_store.ProductCount == 0) {
        printf("No store found that matches all criteria\n");
    } else {
        printf("\nBest store products:\n");
        float total_price = 0.0;
    
        for (int i = 0; i < best_store.ProductCount; i++) {
            struct product* prod = &best_store.products[i];
            total_price += prod->Price;
        
            printf("Product: %s, Price: %.2f, Score: %.1f, Store: %s\n",
                prod->Name, prod->Price, prod->Score, prod->StoreName);
            printf("Do you want to buy from this store? enter 'y' or 'n' ");
            scanf("%49s",&ans);
        // Find matching order quantity and decrease inventory
            if(ans[0] == 'y')
            {
                for (int j = 0; j < data.OrderCount; j++) {
                if (strcmp(prod->Name, data.Orders[j].product_name) == 0) {
                    decrease_entity_count(prod->StoreName, prod->CategoryName, 
                        prod->id, data.Orders[j].quantity);
                    break;
                }
            }
            }
            else if (ans[0] == 'n')
            {
                printf("buying is cancelled, see you for another order");
                return 0;
            }
            
        }
    
        printf("\nTotal Price: %.2f\n", total_price);
    }

    free(orders);
    free(stores);

    // Cleanup logging
    cleanup_logging();

    return 0;
}