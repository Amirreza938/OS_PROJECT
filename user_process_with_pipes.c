#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "data_load.h"  // Ensure this header file includes the struct definitions

// Function to process orders in a thread
void* process_orders(void* arg) {
    struct buyBox* data = (struct buyBox*)arg;
    struct order* orders = data->Orders;
    int order_count = data->OrderCount;

    printf("PID %d created thread for Orders, TID: %lu\n", getpid(), pthread_self());
    
    for (int i = 0; i < order_count; i++) {
        printf("PID %d processing Order: %s, Quantity: %d\n", getpid(), orders[i].product_name, orders[i].quantity);
    }
    
    return NULL;
}

// Structure to pass data for processing a single product
struct process_product_struct {
    struct product *prod;
    struct buyBox UserBuyBox;
    struct sellBox *StoreSellBox;
    int pipe_fd;  // File descriptor for pipe
};

// Function to process a single product
void* process_product(void* arg) {
    // Unpack parameters
    void** params = (void**)arg;

    struct buyBox* UserBuyBox = (struct buyBox*)params[0];
    struct sellBox* StoreSellBox = (struct sellBox*)params[1];
    struct product* prod = (struct product*)params[2];
    int pipe_fd = *(int*)params[3]; // Retrieve pipe file descriptor

    // Free the allocated parameter array
    free(params);

    if (!StoreSellBox->products) {
        StoreSellBox->products = NULL; // Initialize to NULL for realloc compatibility
        StoreSellBox->ProductCount = 0;
    }
    
    for (int i = 0; i < UserBuyBox->OrderCount; i++) {
        if (strcmp(prod->Name, UserBuyBox->Orders[i].product_name) == 0) {
            if (prod->Entity >= UserBuyBox->Orders[i].quantity) {
                struct product* temp = realloc(StoreSellBox->products, 
                                               (StoreSellBox->ProductCount + 1) * sizeof(struct product));
                if (!temp) {
                    perror("Failed to allocate memory for products");
                    return NULL;
                }
                StoreSellBox->products = temp;
                StoreSellBox->products[StoreSellBox->ProductCount] = *prod;
                StoreSellBox->ProductCount++;

                // Write product details to the pipe
                write(pipe_fd, prod, sizeof(struct product));
            
                break;
            }
        }
    }

    return NULL;
}

void process_category(struct category* cat, struct buyBox UserBuyBox, struct sellBox* StoreSellBox, int store_pid, int pipe_fd) {
    printf("PID %d create child for category: %s PID: %d\n", store_pid, cat->Name, getpid());

    pthread_t* threads = malloc(cat->ProductCount * sizeof(pthread_t));
    if (!threads) {
        perror("Failed to allocate memory for threads");
        exit(1);
    }

    // Create threads for each product
    for (int i = 0; i < cat->ProductCount; i++) {
        // Allocate memory for parameter array
        void** params = malloc(4 * sizeof(void*));
        if (!params) {
            perror("Failed to allocate memory for thread parameters");
            free(threads);
            exit(1);
        }

        params[0] = &UserBuyBox;         // Pass UserBuyBox
        params[1] = StoreSellBox;        // Pass pointer to StoreSellBox
        params[2] = &cat->Products[i];   // Pass pointer to the product
        params[3] = &pipe_fd;            // Pass pipe file descriptor

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
            process_category(&s->Categories[i], UserBuyBox, StoreSellBox, getpid(), pipe_fd);
            exit(0);
        } else if (pid > 0) {
            wait(NULL);  // Wait for the category process to finish
        } else {
            perror("fork failed");
            exit(1);
        }
    }
}

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

    pthread_t order_thread;
    if (pthread_create(&order_thread, NULL, process_orders, (void*)&data) != 0) {
        perror("Failed to create thread");
        free(orders);
        exit(1);
    }
    pthread_join(order_thread, NULL);

    // Create a pipe
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe failed");
        free(orders);
        exit(1);
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
            process_store(&stores[i], data, StoreSellBox_ptr, getppid(), pipe_fd[1]);
            // Child does not print products, it writes them to the pipe
            free(StoreSellBox_ptr->products);
            free(StoreSellBox_ptr);
            exit(0);  // Exit after processing the store
        } else if (pid < 0) {
            perror("fork failed");
            free(StoreSellBox_ptr);
            free(orders);
            exit(1);
        }
    }

    // Parent process reads from the pipe
    close(pipe_fd[1]);  // Close write end in the parent
    struct product prod;
    while (read(pipe_fd[0], &prod, sizeof(struct product)) > 0) {
        printf("Product: %s, Quantity: %d, Price: %.2f\n", prod.Name, prod.Entity, prod.Price);
    }

    close(pipe_fd[0]);  // Close read end

    for (int i = 0; i < store_count; i++) {
        wait(NULL);
    }

    free(orders);
    free(stores);

    return 0;
}
