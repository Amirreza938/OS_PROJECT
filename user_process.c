#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

// Structure to hold product order data
struct order {
    char *product_name;
    int quantity;
    float price;
};

// Function to process each order (thread handler)
void* process_order(void* arg) {
    struct order *ord = (struct order*)arg;
    printf("PID %d create thread for Order: %s, Quantity: %d, Price: %.2f\n", getpid(), ord->product_name, ord->quantity, ord->price);
    return NULL;
}

// Function to simulate processing categories under stores
void process_category(const char* store_name, const char* category_name, struct order* orders, int order_count) {
    printf("PID %d create child for Category %s under Store %s\n", getpid(), category_name, store_name);

    pthread_t *threads = (pthread_t*)malloc(order_count * sizeof(pthread_t));

    // Create threads for orders in the category
    for (int i = 0; i < order_count; i++) {
        printf("PID %d create thread for Order %d in %s\n", getpid(), i + 1, category_name);
        pthread_create(&threads[i], NULL, process_order, (void*)&orders[i]);
    }

    // Wait for all threads to finish (not using wait() here)
    for (int i = 0; i < order_count; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
}

// Function to simulate creating stores and processing categories
void process_store(const char* store_name, struct order* orders, int order_count) {
    printf("PID %d create child for Store %s\n", getpid(), store_name);

    // Create child processes for categories in the store
    char* categories[] = {"Digital", "Home", "Apparel", "Food", "Market", "Toys", "Beauty", "Sports"};

    for (int i = 0; i < 8; i++) {
        pid_t pid = fork();
        if (pid == 0) {  // Child process
            process_category(store_name, categories[i], orders, order_count);
            exit(0);  // Exit the child process after processing the category
        } else if (pid > 0) {
            // Parent process continues to create more child processes for categories
            printf("PID %d create child for Category %s under Store %s\n", getpid(), categories[i], store_name);
        } else {
            perror("fork failed");
            exit(1);
        }
    }
}

// Main function
int main() {
    char username[50];
    int order_count;
    float price_threshold;

    // Get user input for username
    printf("Username: ");
    scanf("%s", username);

    // Get user input for order list
    printf("Orderlist0:\n");
    printf("Enter the number of orders: ");
    scanf("%d", &order_count);

    struct order* orders = (struct order*)malloc(order_count * sizeof(struct order));

    // Get orders input
    for (int i = 0; i < order_count; i++) {
        orders[i].product_name = (char*)malloc(100 * sizeof(char));

        // Get the product name and quantity
        printf("Enter product name for order %d: ", i + 1);
        getchar(); // to clear the newline
        fgets(orders[i].product_name, 100, stdin);
        orders[i].product_name[strcspn(orders[i].product_name, "\n")] = 0;  // Remove trailing newline

        printf("Enter quantity for order %d: ", i + 1);
        scanf("%d", &orders[i].quantity);

        printf("Enter price for order %d: ", i + 1);
        scanf("%f", &orders[i].price);
    }

    // Get price threshold input
    printf("Price threshold: ");
    scanf("%f", &price_threshold);

    printf("\nUser: %s\n", username);

    // First, create threads for the orders
    pthread_t *threads = (pthread_t*)malloc(order_count * sizeof(pthread_t));
    for (int i = 0; i < order_count; i++) {
        printf("PID %d create thread for Order %d\n", getpid(), i + 1);
        pthread_create(&threads[i], NULL, process_order, (void*)&orders[i]);
    }

    // Wait for all threads to finish
    for (int i = 0; i < order_count; i++) {
        pthread_join(threads[i], NULL);
    }

    // After threads are done, now create child processes for stores
    char* stores[] = {"Store1", "Store2", "Store3"};

    for (int i = 0; i < 3; i++) {
        pid_t pid = fork();
        if (pid == 0) {  // Child process
            process_store(stores[i], orders, order_count);
            exit(0);  // Exit the child process after processing the store
        } else if (pid > 0) {
            // Parent process continues to create more child processes for stores
            printf("PID %d create child for Store %s\n", getpid(), stores[i]);
        } else {
            perror("fork failed");
            exit(1);
        }
    }

    // Clean up dynamically allocated memory
    for (int i = 0; i < order_count; i++) {
        free(orders[i].product_name);
    }
    free(orders);
    free(threads);

    return 0;
}
