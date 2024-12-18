#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "data_load.h"
#include <sys/mman.h> // For shm_open and mmap
#include <fcntl.h>
#include <semaphore.h>
#include <time.h>
#include <dirent.h>
#include <regex.h>
struct process_params
{
    struct product *prod;
    char *storeName;
    char *categoryName;
    struct buyBox buyBox;
    int pipe_fd;
    char *username;
};

int max_order = 0;

void get_current_time(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}
// Mutex for thread-safe logging
pthread_mutex_t log_mutex;

int get_max_order_number(const char *storeName, const char *categoryName, const char *username)
{
    char directory_path[200];
    snprintf(directory_path, sizeof(directory_path), "./Data_set/%s/%s", storeName, categoryName);

    DIR *dir = opendir(directory_path);
    if (dir == NULL)
    {
        perror("Error opening directory");
        return -1;
    }

    struct dirent *entry;
    int max_order = -1;
    char pattern[100];
    snprintf(pattern, sizeof(pattern), "^%s_Order([0-9]+)\\.log$", username);

    regex_t regex;
    if (regcomp(&regex, pattern, REG_EXTENDED) != 0)
    {
        perror("Error compiling regex");
        closedir(dir);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        regmatch_t match[2];
        if (regexec(&regex, entry->d_name, 2, match, 0) == 0)
        {
            char order_number_str[10];
            int length = match[1].rm_eo - match[1].rm_so;
            strncpy(order_number_str, entry->d_name + match[1].rm_so, length);
            order_number_str[length] = '\0';

            int order_number = atoi(order_number_str);
            if (order_number > max_order)
            {
                max_order = order_number;
            }
        }
    }

    regfree(&regex);
    closedir(dir);

    return max_order;
}

void write_log(const char *username, const char *message , const char * storeName , const char * categoryName)
{
    // Create the log file name based on the username
    char log_filename[100];
    
    snprintf(log_filename, sizeof(log_filename), "./Data_set/%s/%s/%s_Order%d.log",storeName , categoryName, username,max_order);

    // Mutex for thread-safe logging
    pthread_mutex_lock(&log_mutex); // Lock the mutex for thread safety

    FILE *log_file = fopen(log_filename, "a");
    if (log_file != NULL)
    {
        // Write log message with timestamp
        char time_buffer[64];
        get_current_time(time_buffer, sizeof(time_buffer));
        fprintf(log_file, "[%s] %s\n", time_buffer, message);
        fclose(log_file);
    }
    else
    {
        perror("Error opening log file");
    }

    pthread_mutex_unlock(&log_mutex); // Unlock the mutex
}

// Initialize logging system
void init_logging()
{
    pthread_mutex_init(&log_mutex, NULL);       // Initialize the mutex
    FILE *log_file = fopen("logfile.txt", "w"); // Clear log file at program start
    if (log_file != NULL)
    {
        fclose(log_file);
    }
    else
    {
        perror("Error initializing log file");
    }
}

// Clean up logging system
void cleanup_logging()
{
    pthread_mutex_destroy(&log_mutex); // Destroy the mutex
}
void decrease_entity_count(const char *store_name, const char *category_name, int product_id, int quantity)
{
    // Construct the file path
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "./Data_set/%s/%s/%d.txt", store_name, category_name, product_id);

    // Open the file for reading and writing
    FILE *file = fopen(file_path, "r+");
    if (file == NULL)
    {
        perror("Error opening file");
        return;
    }

    struct product prod = {0};
    char line[256];
    int entity_found = 0;

    // Read product details
    while (fgets(line, sizeof(line), file))
    {
        if (sscanf(line, "Name: %[^\n]", prod.Name) == 1)
            continue;
        if (sscanf(line, "Price: %f", &prod.Price) == 1)
            continue;
        if (sscanf(line, "Score: %f", &prod.Score) == 1)
            continue;
        if (sscanf(line, "Entity: %d", &prod.Entity) == 1)
        {
            entity_found = 1;
            continue;
        }
        if (sscanf(line, "Last Modified: %[^\n]", prod.LastModified) == 1)
            continue;
    }

    // Ensure "Entity" field is found in the file
    if (!entity_found)
    {
        printf("Entity count not found in the file.\n");
        fclose(file);
        return;
    }

    // Check if enough entities are available
    if (prod.Entity < quantity)
    {
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
void *process_product(void *arg)
{
    // Unpack parameters
    struct process_params *params = (struct process_params *)arg;
    struct product *prod = params->prod;
    int pipe_fd = params->pipe_fd;
    const char *username = params->username;
    // Log thread start and file being processed
    char log_message[512];
    snprintf(log_message, sizeof(log_message), "Thread started for file: ./Data_set/%s/%s/%d.txt",
             params->storeName, params->categoryName, prod->id);
    write_log(username, log_message,params->storeName,params->categoryName);

    for (int i = 0; i < params->buyBox.OrderCount; i++)
    {
        if (strcmp(prod->Name, params->buyBox.Orders[i].product_name) == 0)
        {
            if (prod->Entity >= params->buyBox.Orders[i].quantity)
            {
                // Log product found
                snprintf(log_message, sizeof(log_message), "Thread found product: %s in store: %s, category: %s",
                         prod->Name, params->storeName, params->categoryName);
                write_log(username, log_message,params->storeName,params->categoryName);

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
float calculateScore(struct sellBox *box)
{
    float totalScore = 0.0;
    for (int i = 0; i < box->ProductCount; i++)
    {
        totalScore += box->products[i].Score * box->products[i].Price;
    }
    return totalScore;
}

struct sellBox getBestBox(int pipe_fd[][2], int store_count, float price_threshold)
{
    struct sellBox userSellBox;
    userSellBox.products = NULL;
    userSellBox.ProductCount = 0;

    struct sellBox *allStoreBoxes = malloc(store_count * sizeof(struct sellBox));
    float *totalPrices = malloc(store_count * sizeof(float));
    int validStoreCount = 0;

    // Read data from all stores
    for (int i = 0; i < store_count; i++)
    {
        close(pipe_fd[i][1]); // Close write end

        allStoreBoxes[i].ProductCount = 0;
        allStoreBoxes[i].products = malloc(0);
        totalPrices[i] = 0.0;

        struct product prod;
        while (read(pipe_fd[i][0], &prod, sizeof(struct product)) > 0)
        {
            allStoreBoxes[i].ProductCount++;
            allStoreBoxes[i].products = realloc(allStoreBoxes[i].products,
                                                allStoreBoxes[i].ProductCount * sizeof(struct product));

            allStoreBoxes[i].products[allStoreBoxes[i].ProductCount - 1] = prod;
            totalPrices[i] += prod.Price;
        }

        close(pipe_fd[i][0]);

        // Check if this store has all products and is within price threshold
        if (totalPrices[i] <= price_threshold || price_threshold == -1)
        {
            validStoreCount++;
        }
    }

    // Find the best store among valid ones
    float bestScore = -1.0;
    int bestStoreIndex = -1;

    for (int i = 0; i < store_count; i++)
    {
        if (totalPrices[i] <= price_threshold || price_threshold == -1)
        {
            float currentScore = calculateScore(&allStoreBoxes[i]);
            if (currentScore > bestScore)
            {
                bestScore = currentScore;
                bestStoreIndex = i;
            }
        }
    }

    // Copy the best store's data
    if (bestStoreIndex != -1)
    {
        userSellBox = allStoreBoxes[bestStoreIndex];
    }

    // Free memory for other stores
    for (int i = 0; i < store_count; i++)
    {
        if (i != bestStoreIndex)
        {
            free(allStoreBoxes[i].products);
        }
    }

    free(allStoreBoxes);
    free(totalPrices);

    return userSellBox;
}
// Function to process a category
void process_category(struct category *cat, struct buyBox UserBuyBox, struct sellBox *StoreSellBox, int store_pid, int pipe_fd, char StoreName[256], const char *username)
{

    max_order = get_max_order_number(StoreName,cat->Name,username);
    if (max_order < 0)
    {
        max_order = 0; // Default to 0 if no logs found
    }
    else
    {
        max_order++; // Increment for the new order
    }

    printf("PID %d create child for category: %s PID: %d\n", store_pid, cat->Name, getpid());

    pthread_t *threads = malloc(cat->ProductCount * sizeof(pthread_t));
    if (!threads)
    {
        perror("Failed to allocate memory for threads");
        exit(1);
    }

    // Create threads for each product
    for (int i = 0; i < cat->ProductCount; i++)
    {
        // Allocate memory for parameter array
        struct process_params *params = malloc(sizeof(struct process_params));
        if (!params)
        {
            perror("Failed to allocate memory for thread parameters");
            free(threads);
            exit(EXIT_FAILURE);
        }

        params->prod = &cat->Products[i]; // Pass pointer to the product
        params->storeName = StoreName;    // Pass store name
        params->categoryName = cat->Name; // Pass category name
        params->pipe_fd = pipe_fd;        // Pass pipe file descriptor
        params->buyBox = UserBuyBox;
        params->username = username;

        if (pthread_create(&threads[i], NULL, process_product, (void *)params) != 0)
        {
            perror("Failed to create thread for product");
            free(threads);
            free(params);
            exit(1);
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < cat->ProductCount; i++)
    {
        pthread_join(threads[i], NULL);
    }

    free(threads);
}

// Function to handle a store
void process_store(struct store *s, struct buyBox UserBuyBox, struct sellBox *StoreSellBox, int parent_pid, int pipe_fd, const char *username)
{
    printf("PID %d created child for store: %s\n", parent_pid, s->Name);

    for (int i = 0; i < s->CategoryCount; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        { // Child process for categories
            process_category(&s->Categories[i], UserBuyBox, StoreSellBox, getpid(), pipe_fd, s->Name, username);
            exit(0);
        }
        else if (pid > 0)
        {
            wait(NULL);
        }
        else
        {
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
void log_purchase_history(const char *username, const char *store_name)
{
    char history_filename[100];
    snprintf(history_filename, sizeof(history_filename), "%s_purchase_history.txt", username);

    FILE *history_file = fopen(history_filename, "a");
    if (history_file != NULL)
    {
        fprintf(history_file, "%s\n", store_name);
        fclose(history_file);
    }
    else
    {
        perror("Error opening purchase history file");
    }
}
int has_purchased_from_store(const char *username, const char *store_name)
{
    char history_filename[100];
    snprintf(history_filename, sizeof(history_filename), "%s_purchase_history.txt", username);

    FILE *history_file = fopen(history_filename, "r");
    if (history_file == NULL)
    {
        return 0; // If file doesn't exist, return false (user hasn't bought from any store yet)
    }

    char line[256];
    while (fgets(line, sizeof(line), history_file))
    {
        line[strcspn(line, "\n")] = '\0'; // Remove the newline character
        if (strcmp(line, store_name) == 0)
        {
            fclose(history_file);
            return 1; // User has purchased from this store before
        }
    }

    fclose(history_file);
    return 0; // User hasn't purchased from this store
}

struct rating_params
{
    char store_name[256];
    char category_name[256];
    int product_id;
    float user_rating;
};

void *update_product_score_thread(void *arg)
{
    struct rating_params *params = (struct rating_params *)arg;

    // Construct the file path
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "./Data_set/%s/%s/%d.txt",
             params->store_name, params->category_name, params->product_id);

    // Open the file for reading and writing
    FILE *file = fopen(file_path, "r+");
    if (file == NULL)
    {
        perror("Error opening product file for rating update");
        return NULL;
    }

    struct product prod = {0};
    char line[256];
    int score_found = 0;

    // Read the product details
    while (fgets(line, sizeof(line), file))
    {
        if (sscanf(line, "Name: %[^\n]", prod.Name) == 1)
            continue;
        if (sscanf(line, "Price: %f", &prod.Price) == 1)
            continue;
        if (sscanf(line, "Score: %f", &prod.Score) == 1)
        {
            score_found = 1;
            continue;
        }
        if (sscanf(line, "Entity: %d", &prod.Entity) == 1)
            continue;
        if (sscanf(line, "Last Modified: %[^\n]", prod.LastModified) == 1)
            continue;
    }

    if (!score_found)
    {
        printf("Score field not found for product ID: %d\n", params->product_id);
        fclose(file);
        return NULL;
    }

    // Calculate the new score
    prod.Score = (prod.Score + params->user_rating) / 2.0;

    // Move the file pointer to the beginning for overwriting
    rewind(file);

    // Write updated product details back to the file
    fprintf(file, "Name: %s\n", prod.Name);
    fprintf(file, "Price: %.2f\n", prod.Price);
    fprintf(file, "Score: %.1f\n", prod.Score);
    fprintf(file, "Entity: %d\n", prod.Entity);
    fprintf(file, "Last Modified: %s\n", prod.LastModified);

    printf("Updated Score for product '%s': %.1f\n", prod.Name, prod.Score);

    fclose(file);
    free(params); // Free the allocated memory for parameters
    return NULL;
}

void prompt_and_update_ratings_threaded(const struct sellBox *best_store, const struct buyBox *user_orders)
{
    printf("\nPlease rate the products you purchased (scale 1-5):\n");

    pthread_t *threads = malloc(best_store->ProductCount * sizeof(pthread_t));
    if (!threads)
    {
        perror("Failed to allocate memory for threads");
        return;
    }

    for (int i = 0; i < best_store->ProductCount; i++)
    {
        struct product *prod = &best_store->products[i];

        // Check if the product was purchased
        for (int j = 0; j < user_orders->OrderCount; j++)
        {
            if (strcmp(prod->Name, user_orders->Orders[j].product_name) == 0)
            {
                float user_rating = 0.0;

                // Prompt the user for a rating
                printf("Rate the product '%s' (Price: %.2f, Store: %s): ", prod->Name, prod->Price, prod->StoreName);
                scanf("%f", &user_rating);

                if (user_rating < 1 || user_rating > 5)
                {
                    printf("Invalid rating. Please enter a value between 1 and 5.\n");
                    j--; // Re-prompt for this product
                    continue;
                }

                // Allocate memory for thread parameters
                struct rating_params *params = malloc(sizeof(struct rating_params));
                if (!params)
                {
                    perror("Failed to allocate memory for rating parameters");
                    continue;
                }

                // Populate parameters for the thread
                strcpy(params->store_name, prod->StoreName);
                strcpy(params->category_name, prod->CategoryName);
                params->product_id = prod->id;
                params->user_rating = user_rating;

                // Create a thread to update the rating
                if (pthread_create(&threads[i], NULL, update_product_score_thread, params) != 0)
                {
                    perror("Failed to create thread for rating update");
                    free(params);
                    continue;
                }
                break;
            }
        }
    }

    // Wait for all threads to finish
    for (int i = 0; i < best_store->ProductCount; i++)
    {
        pthread_join(threads[i], NULL);
    }

    free(threads);
}
int main()
{
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

    struct order *orders = malloc(order_count * sizeof(struct order));
    if (!orders)
    {
        perror("Failed to allocate memory for orders");
        exit(1);
    }

    printf("Enter OrderList (product_name quantity):\n");
    for (int i = 0; i < order_count; i++)
    {
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
    for (int i = 0; i < store_count; i++)
    {
        if (pipe(pipe_fd[i]) == -1)
        {
            perror("pipe failed");
            free(orders);
            exit(1);
        }
    }

    for (int i = 0; i < store_count; i++)
    {
        pid_t pid = fork();

        struct sellBox *StoreSellBox_ptr = malloc(sizeof(struct sellBox));
        if (!StoreSellBox_ptr)
        {
            perror("Failed to allocate memory for StoreSellBox");
            free(orders);
            exit(1);
        }
        StoreSellBox_ptr->products = NULL; // Ensure products pointer is initialized
        StoreSellBox_ptr->ProductCount = 0;

        if (pid == 0)
        { // Child process for store
            process_store(&stores[i], data, StoreSellBox_ptr, getppid(), pipe_fd[i][1], username);
            // Child does not print products, it writes them to the pipe
            free(StoreSellBox_ptr->products);
            free(StoreSellBox_ptr);
            exit(0); // Exit after processing the store
        }
        else if (pid < 0)
        {
            perror("fork failed");
            free(StoreSellBox_ptr);
            free(orders);
            exit(1);
        }
    }

    for (int i = 0; i < store_count; i++)
    {
        wait(NULL);
    }
    // In main()
    struct sellBox best_store = getBestBox(pipe_fd, store_count, price_threshold);

    if (best_store.ProductCount == 0)
    {
        printf("No store found that matches all criteria\n");
    }
    else
    {
        // Display all products from the best store
        printf("\nBest store products:\n");
        float total_price = 0.0;
        float discount = 0.0;

        // Iterate over all products in the best store and calculate total price
        for (int i = 0; i < best_store.ProductCount; i++)
        {
            struct product *prod = &best_store.products[i];

            // Display product details
            printf("Product: %s, Price: %.2f, Score: %.1f, Store: %s\n",
                   prod->Name, prod->Price, prod->Score, prod->StoreName);

            // Accumulate the total price for all products in the order list
            total_price += (prod->Price * orders[i].quantity);
        }

        // Ask if the user wants to buy from this store
        printf("\nDo you want to buy from this store? Enter 'y' or 'n': ");
        char ans[2];
        scanf("%2s", ans);

        if (ans[0] == 'y')
        {
            // Check if the user qualifies for a discount (repeat visit)
            if (has_purchased_from_store(username, best_store.products->StoreName))
            {
                discount = 0.05; // 5% discount for repeat customers
                printf("You get a 5%% discount for re-entering this store!\n");
            }

            // Apply the discount to the total price
            float final_price = total_price * (1 - discount);

            // Process each product in the order list
            for (int i = 0; i < best_store.ProductCount; i++)
            {
                struct product *prod = &best_store.products[i];

                // Decrease inventory for the purchased product
                for (int j = 0; j < data.OrderCount; j++)
                {
                    if (strcmp(prod->Name, data.Orders[j].product_name) == 0)
                    {
                        decrease_entity_count(prod->StoreName, prod->CategoryName,
                                              prod->id, data.Orders[j].quantity);
                        break;
                    }
                }

                // Log the purchase to the history
                log_purchase_history(username, prod->StoreName);
            }

            // Display the final total price after all purchases
            printf("\nTotal Price after discount (if applicable): %.2f\n", final_price);
        }
        else
        {
            printf("\nYou chose not to buy from this store. See you next time!\n");
            return 0;
        }
        prompt_and_update_ratings_threaded(&best_store, &data);
    }

    free(orders);
    free(stores);

    // Cleanup logging
    cleanup_logging();

    return 0;
}