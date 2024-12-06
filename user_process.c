#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

// Structure to hold product order data
struct order {
    char product_name[100];
    int quantity;
};

// Function to process orders in a thread
void* process_orders(void* arg) {
    struct order* orders = (struct order*)arg;
    printf("PID %d create thread for Orders TID: %lu\n", getpid(), pthread_self());
    for (int i = 0; i < 3; i++) {  // Example assumes 3 orders
        printf("PID %d create thread for Order: %s %d\n", getpid(), orders[i].product_name, orders[i].quantity);
    }
    return NULL;
}

// Function to handle categories within a store
void process_category(const char* category_name, int store_pid) {
    printf("PID %d create child for %s PID: %d\n", store_pid, category_name, getpid());
}

// Function to handle a store
void process_store(const char* store_name, struct order* orders, int store_pid) {
    printf("PID %d create child for %s PID: %d\n", store_pid, store_name, getpid());

    // List of categories
    const char* categories[] = {"Digital", "Home", "Apparel", "Food", "Market", "Toys", "Beauty", "Sports"};
    int num_categories = sizeof(categories) / sizeof(categories[0]);

    // Create a process for each category
    for (int i = 0; i < num_categories; i++) {
        pid_t pid = fork();
        if (pid == 0) {  // Child process for categories
            process_category(categories[i], getppid());
            exit(0);
        } else if (pid > 0) {
            wait(NULL);  // Wait for the category process to finish
        } else {
            perror("fork failed");
            exit(1);
        }
    }
}

// Main function
int main() {
    char username[50];
    int order_count = 3;  // Fixed for the example
    float price_threshold;

    // Input: Username
    printf("Username: ");
    scanf("%49s", username);

    // Input: Orders
    struct order orders[3];
    printf("OrderList0:\n");
    for (int i = 0; i < order_count; i++) {
        scanf("%s %d", orders[i].product_name, &orders[i].quantity);
    }

    // Input: Price threshold
    printf("Price threshold: ");
    scanf("%f", &price_threshold);

    printf("\n%s create PID: %d\n", username, getpid());

    // Create a thread for orders
    pthread_t order_thread;
    if (pthread_create(&order_thread, NULL, process_orders, (void*)orders) != 0) {
        perror("Failed to create thread");
        exit(1);
    }
    pthread_join(order_thread, NULL);  // Wait for the orders thread to finish

    // Create child processes for each store
    const char* stores[] = {"Store1", "Store2", "Store3"};
    int num_stores = sizeof(stores) / sizeof(stores[0]);

    for (int i = 0; i < num_stores; i++) {
        pid_t pid = fork();
        if (pid == 0) {  // Child process for store
            process_store(stores[i], orders, getppid());
            exit(0);
        } else if (pid > 0) {
            wait(NULL);  // Wait for the store process to finish
        } else {
            perror("fork failed");
            exit(1);
        }
    }

    return 0;
}