#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "data_load.h"  // Include the dataset loader


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
// Structure to store aggregated results for a store
struct storeResult {
    char storeName[100];
    float totalPrice;
    char matchedOrders[1024];
};

void process_store(struct store* s, struct buyBox UserBuyBox, int pipe_fd) {
    struct storeResult result = {0};
    struct storeResult price_result = {0};
    strcpy(result.storeName, s->Name);
    result.totalPrice = 0;
    result.matchedOrders[0] = '\0';
    float storeValue = 0;  // Initialize value for this store

    for (int i = 0; i < s->CategoryCount; i++) {
        for (int j = 0; j < s->Categories[i].ProductCount; j++) {
            struct product* prod = &s->Categories[i].Products[j];
            for (int k = 0; k < UserBuyBox.OrderCount; k++) {
                if (strcmp(prod->Name, UserBuyBox.Orders[k].product_name) == 0 && prod->Entity >= UserBuyBox.Orders[k].quantity) {
                    result.totalPrice += prod->Price * UserBuyBox.Orders[k].quantity;

                    // Calculate the value of this order (score * price)
                    storeValue += (prod->Score * prod->Price) * UserBuyBox.Orders[k].quantity;

                    // Append matched order details to the result
                    char orderDetails[100];
                    snprintf(orderDetails, sizeof(orderDetails), "%s(%d), ", prod->Name, UserBuyBox.Orders[k].quantity);
                    strcat(result.matchedOrders, orderDetails);
                }
            }
        }
    }

    // Trim trailing ", " if there are matched orders
    size_t len = strlen(result.matchedOrders);
    if (len > 0 && result.matchedOrders[len - 2] == ',') {
        result.matchedOrders[len - 2] = '\0';  // Remove trailing ", "
    }

    // Add store value to the result
    result.totalPrice = storeValue;

    write(pipe_fd, &result, sizeof(struct storeResult));
}
int main() {
    char username[50];
    int order_count;
    float price_threshold;

    fgets(username, sizeof(username), stdin);
    scanf("%d", &order_count);

    struct order* orders = malloc(order_count * sizeof(struct order));
    if (!orders) {
        perror("Failed to allocate memory for orders");
        exit(1);
    }

    for (int i = 0; i < order_count; i++) {
        scanf("%s %d", orders[i].product_name, &orders[i].quantity);
    }

    if (scanf("%f", &price_threshold) == 0) {
        price_threshold = -1;  // No threshold
    }

    struct buyBox UserBuyBox = {orders, order_count};

    struct store* stores;
    int store_count;
    get_stores("./Data_set", &stores, &store_count);

    // Create a pipe
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe failed");
        free(orders);
        exit(1);
    }

    for (int i = 0; i < store_count; i++) {
        pid_t pid = fork();
        if (pid == 0) {  // Child process for store
            close(pipe_fd[0]);
            process_store(&stores[i], UserBuyBox, pipe_fd[1]);
            exit(0);
        } else if (pid < 0) {
            perror("fork failed");
            exit(1);
        }
    }

    close(pipe_fd[1]);  // Parent closes write end
    struct storeResult result;

    printf("\nResults:\n");
    while (read(pipe_fd[0], &result, sizeof(struct storeResult)) > 0) {
        if (result.totalPrice > 0) {  // Only print if there are matched orders
            printf("Store: %s, Total Price: %.2f, Orders: %s\n", result.storeName, result.totalPrice, result.matchedOrders);
        }
    }

    close(pipe_fd[0]);
    for (int i = 0; i < store_count; i++) {
        wait(NULL);
    }

    free(orders);
    free(stores);
    return 0;
}
