#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// Structure to hold product data
struct product {
    char *Name;
    float Price;
    float Score;
    int Entity;
    char LastModified[64]; // Field to store last modified date and time
};

// Structure to hold category data
struct category {
    char *Name;
    struct product *Products;  // Array of products in the category
    int ProductCount;
};

// Structure to hold store data
struct store {
    char *Name;
    struct category *Categories;  // Array of categories in the store
    int CategoryCount;
};

// Function to read a file and parse its content into a product
struct product read_file(const char *filepath) {
    struct product p;
    p.Name = (char*)malloc(256); // Allocate memory for Name

    FILE *file = fopen(filepath, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE); // Exit on file open failure
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file)) {
        if (strncmp(buffer, "Name:", 5) == 0) {
            sscanf(buffer, "Name: %[^\n]", p.Name);
        } else if (strncmp(buffer, "Price:", 6) == 0) {
            sscanf(buffer, "Price: %f", &p.Price);
        } else if (strncmp(buffer, "Score:", 6) == 0) {
            sscanf(buffer, "Score: %f", &p.Score);
        } else if (strncmp(buffer, "Entity:", 7) == 0) {
            sscanf(buffer, "Entity: %d", &p.Entity);
        } else if (strncmp(buffer, "Last Modified:", 14) == 0) {
            sscanf(buffer, "Last Modified: %[^\n]", p.LastModified);
        }
    }

    fclose(file);
    return p;
}

// Function to extract the name of the directory (category or store)
char *get_directory_name(const char *dirpath) {
    char *dir_name = strrchr(dirpath, '/');
    if (dir_name) {
        dir_name++;  // Move past the '/' character
    } else {
        dir_name = (char *)dirpath;
    }
    return dir_name;
}

// Function to traverse directories and process files (for categories)
struct category traverse_category(const char *dirpath) {
    struct category cat;
    cat.Name = get_directory_name(dirpath); // Extract category name
    cat.Products = NULL;
    cat.ProductCount = 0;

    struct dirent *entry;
    DIR *dp = opendir(dirpath);

    if (dp == NULL) {
        perror("Error opening category directory");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dp)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Construct full file path
        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, entry->d_name);

        struct stat path_stat;
        stat(filepath, &path_stat);

        if (S_ISREG(path_stat.st_mode)) {
            // Process as product file
            cat.Products = realloc(cat.Products, sizeof(struct product) * (cat.ProductCount + 1));
            cat.Products[cat.ProductCount] = read_file(filepath);
            cat.ProductCount++;
        }
    }

    closedir(dp);
    return cat;
}

// Function to traverse directories and process files (for stores)
struct store traverse_store(const char *dirpath) {
    struct store s;
    s.Name = get_directory_name(dirpath); // Extract store name
    s.Categories = NULL;
    s.CategoryCount = 0;

    struct dirent *entry;
    DIR *dp = opendir(dirpath);

    if (dp == NULL) {
        perror("Error opening store directory");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dp)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Construct full category path
        char subdirpath[1024];
        snprintf(subdirpath, sizeof(subdirpath), "%s/%s", dirpath, entry->d_name);

        struct stat path_stat;
        stat(subdirpath, &path_stat);

        if (S_ISDIR(path_stat.st_mode)) {
            // Process as category directory
            s.Categories = realloc(s.Categories, sizeof(struct category) * (s.CategoryCount + 1));
            s.Categories[s.CategoryCount] = traverse_category(subdirpath);
            s.CategoryCount++;
        }
    }

    closedir(dp);
    return s;
}

int main() {
    const char *root_path = "./Data_set"; // Root directory for the dataset

    struct dirent *entry;
    DIR *dp = opendir(root_path);

    if (dp == NULL) {
        perror("Error opening root directory");
        exit(EXIT_FAILURE);
    }

    printf("Dataset Structure:\n");

    while ((entry = readdir(dp)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Construct full store path
        char store_path[1024];
        snprintf(store_path, sizeof(store_path), "%s/%s", root_path, entry->d_name);

        struct stat path_stat;
        stat(store_path, &path_stat);

        if (S_ISDIR(path_stat.st_mode)) {
            // Process as store directory
            struct store s = traverse_store(store_path);

            printf("Store: %s\n", s.Name);
            for (int i = 0; i < s.CategoryCount; i++) {
                struct category *cat = &s.Categories[i];
                printf("  Category: %s\n", cat->Name);
                for (int j = 0; j < cat->ProductCount; j++) {
                    struct product *p = &cat->Products[j];
                    printf("    Product: %s\n", p->Name);
                    printf("      Price: %.2f\n", p->Price);
                    printf("      Score: %.2f\n", p->Score);
                    printf("      Entity: %d\n", p->Entity);
                    printf("      Last Modified: %s\n", p->LastModified);
                }
            }
            free(s.Categories); // Free allocated memory
        }
    }

    closedir(dp);
    return 0;
}
