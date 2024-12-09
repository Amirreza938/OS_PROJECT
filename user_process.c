#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "data_load.h"  // Ensure this header file includes the struct definitions



// Function to process orders in a thread
void* process_orders(void* arg) {
    struct buyBox* data = (struct buyBox*)arg;
    struct order* orders = data->Orders;
    int order_count = data->OrderCount;

    printf("PID %d create thread for Orders TID: %lu\n", getpid(), pthread_self());
    
    for (int i = 0; i < order_count; i++) {
        printf("PID %d processing Order: %s Quantity: %d\n", getpid(), orders[i].product_name, orders[i].quantity);
    }
    
    return NULL;
}

struct process_product_struct
{
    struct product *prod;
    struct buyBox UserBuyBox;
    struct sellBox *StoreBuyBox;

};

// Function to process a single product
void* process_product(void *arg) {
    struct process_product_struct *pps = (struct process_product_struct*) arg;
    
    struct product *product_ptr = pps->prod;
    struct buyBox UserBuyBox = pps->UserBuyBox;
    struct sellBox* StoreBuyBox_ptr = pps->StoreBuyBox;


     if (StoreBuyBox_ptr->products == NULL) {
        StoreBuyBox_ptr->products = malloc(0);  // Start with a valid memory location
        StoreBuyBox_ptr->ProductCount = 0;     // Ensure count is initialized
    }

    for (int i = 0; i < UserBuyBox.OrderCount; i++)
    {
        if(strcmp(product_ptr->Name , UserBuyBox.Orders[i].product_name)==0)
        {
            if (product_ptr->Entity >= UserBuyBox.Orders[i].quantity)
            {
                StoreBuyBox_ptr->products = (struct product*)realloc(StoreBuyBox_ptr->products ,(StoreBuyBox_ptr->ProductCount+1) * sizeof(struct product));
                
                if(!StoreBuyBox_ptr->products){
                    perror("Failed to allocate memory for products");
                    return NULL;
                }

                StoreBuyBox_ptr->products[StoreBuyBox_ptr->ProductCount] = *product_ptr;
                StoreBuyBox_ptr->ProductCount++;
                break;
            }
        }
    }
    
    
}

// Modified process_category function
void process_category(struct category* cat,struct buyBox UserBuyBox,struct sellBox *StoreBuyBox, int store_pid) {
    printf("PID %d create child for category: %s PID: %d\n", store_pid, cat->Name, getpid());

    pthread_t* threads = malloc(cat->ProductCount * sizeof(pthread_t));

    if (!threads) {
        perror("Failed to allocate memory for threads or scores");
        exit(1);
    }

    
    // Create threads for each product
    for (int i = 0; i < cat->ProductCount; i++) {
        struct process_product_struct pps;
        pps.prod = &cat->Products[i];
        pps.UserBuyBox=UserBuyBox;
        pps.StoreBuyBox=StoreBuyBox;

        if (pthread_create(&threads[i], NULL, process_product,(void*)&pps) != 0) {
            perror("Failed to create thread for product");
            free(threads);
            exit(1);
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < cat->ProductCount; i++) {
        pthread_join(threads[i], NULL);
    }

    // Free allocated memory
    free(threads);
}


// Function to handle a store
void process_store(struct store *s, struct buyBox UserBuyBox,struct sellBox *StoreBuyBox, int parent_pid) {
    printf("PID %d create child for store: %s PID: %d\n", parent_pid, s->Name, getpid());

    for (int i = 0; i < s->CategoryCount; i++) {
        pid_t pid = fork();
        if (pid == 0) {  // Child process for categories
            process_category(&s->Categories[i],UserBuyBox,StoreBuyBox, getpid());
            exit(0);
        } else if (pid > 0) {
            wait(NULL);  // Wait for the category process to finish
        } else {
            perror("fork failed");
            exit(1);
        }
    }
    exit(0);  // Exit after processing all categories
}

// int main() {
//     char username[50];
//     int order_count;

//     // Input: Username
//     printf("Username: ");
//     scanf("%49s", username);
    
//     // Input: Number of Orders
//     printf("Enter number of orders: ");
//     scanf("%d", &order_count);
    
//     // Dynamic allocation for orders based on user input
//     struct order* orders = malloc(order_count * sizeof(struct order));
    
//     if (orders == NULL) {
//         perror("Failed to allocate memory for orders");
//         exit(1);
//     }

//     // Input: Orders
//     printf("Enter OrderList (product_name quantity):\n");
//     for (int i = 0; i < order_count; i++) {
//         scanf("%s %d", orders[i].product_name, &orders[i].quantity);
//     }

//     // Input: Price threshold (if needed)
//     float price_threshold;
//     printf("Price threshold: ");
//     scanf("%f", &price_threshold);

//     printf("\n%s create PID: %d\n", username, getpid());

//     // Load dataset
//     struct store *stores = NULL;
//     int store_count = 0;
    
//     get_stores("./Data_set", &stores, &store_count);

//    // Create a structure to pass both orders and their count to the thread
//    struct buyBox data;
//    data.Orders = orders;
//    data.OrderCount = order_count;

//    // Create a thread for processing orders
//    pthread_t order_thread;
   
//    if (pthread_create(&order_thread, NULL, process_orders, (void*)&data) != 0) {
//        perror("Failed to create thread");
//        free(orders);  // Free allocated memory before exiting
//        exit(1);
//    }
   
//    pthread_join(order_thread, NULL);  // Wait for the orders thread to finish

//    // Create child processes for each store
//    for (int i = 0; i < store_count; i++) {
//        pid_t pid = fork();
       
//        struct sellBox *StoreSellBox_ptr = (struct sellBox*) malloc(sizeof(struct sellBox));

//        if (pid == 0) {  // Child process for store
//            process_store(&stores[i], data, StoreSellBox_ptr , getppid());

//             for (int j = 0; j < StoreSellBox_ptr->ProductCount; j++)
//             {
//                 printf("Store %d : product of %s\n",i, StoreSellBox_ptr->products[j]);
//             }
            

//            exit(0);  // Exit after processing the store
//        } else if (pid < 0) {
//            perror("fork failed");
//            free(orders);  // Free allocated memory before exiting
//            exit(1);
//        }
//    }

//    // Wait for all store processes to finish
//    for (int i = 0; i < store_count; i++) {
//        wait(NULL);
//    }

//    // Free allocated memory for orders and stores
//    free(orders);
//    for (int i = 0; i < store_count; i++) {
//        for (int j = 0; j < stores[i].CategoryCount; j++) {
//            free(stores[i].Categories[j].Products); // Free products if dynamically allocated
//        }
//        free(stores[i].Categories); // Free categories array
//    }
//    free(stores); // Free stores array

//    return 0;
// }