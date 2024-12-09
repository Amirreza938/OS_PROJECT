#ifndef DATASET_LOADER_H
#define DATASET_LOADER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// Structure definitions
struct product {
    char Name[256];
    float Price;
    float Score;
    int Entity;
    char LastModified[64];
    int id;
};

struct category {
    char Name[256];
    struct product *Products;
    int ProductCount;
};
struct store {
    char Name[256];
    struct category *Categories;
    int CategoryCount;
};
struct order {
    char product_name[100];
    int quantity;
};

struct buyBox
{
    struct order* Orders;
    int OrderCount;
};

struct sellBox
{
    struct product* products;
    int ProductCount;
};

// Function to read a file and parse its content into a product
struct product read_file(const char *filepath) {
    struct product p = {0};
    FILE *file = fopen(filepath, "r");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Extract the file name without the path
    const char *filename = strrchr(filepath, '/');  // Find the last '/' in the filepath
    if (!filename) {
        filename = filepath;  // If no '/' is found, filename is the entire filepath
    } else {
        filename++;  // Skip the '/'
    }

    // Remove the ".txt" extension from the filename
    char filename_noext[256];
    strncpy(filename_noext, filename, sizeof(filename_noext) - 1);
    filename_noext[sizeof(filename_noext) - 1] = '\0';

    // Find the position of the ".txt" extension and replace it with a null character
    char *dot_pos = strrchr(filename_noext, '.');
    if (dot_pos) {
        *dot_pos = '\0';
    }

    // Assign the file name (converted to an integer) as the product's ID
    sscanf(filename_noext, "%d", &p.id);

    // Parse the file contents for other product fields
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

// Function to extract the directory name
void get_directory_name(const char *dirpath, char *dest) {
    const char *dir_name = strrchr(dirpath, '/');  // strrchr returns const char*
    if (dir_name) {
        dir_name++;  // Skip '/'
    } else {
        dir_name = dirpath;
    }
    strcpy(dest, dir_name);  // Copy the string into dest
}

// Function to traverse category directories
struct category traverse_category(const char *dirpath) {
    struct category cat = {0};
    get_directory_name(dirpath, cat.Name);

    struct dirent *entry;
    DIR *dp = opendir(dirpath);
    if (!dp) {
        perror("Error opening category directory");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, entry->d_name);

        struct stat path_stat;
        stat(filepath, &path_stat);

        if (S_ISREG(path_stat.st_mode)) {
            cat.Products = (struct product*)realloc(cat.Products, sizeof(struct product) * (cat.ProductCount + 1));
            cat.Products[cat.ProductCount] = read_file(filepath);
            cat.ProductCount++;
        }
    }

    closedir(dp);
    return cat;
}

// Function to traverse store directories
struct store traverse_store(const char *dirpath) {
    struct store s = {0};
    get_directory_name(dirpath, s.Name);

    struct dirent *entry;
    DIR *dp = opendir(dirpath);
    if (!dp) {
        perror("Error opening store directory");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char subdirpath[1024];
        snprintf(subdirpath, sizeof(subdirpath), "%s/%s", dirpath, entry->d_name);

        struct stat path_stat;
        stat(subdirpath, &path_stat);

        if (S_ISDIR(path_stat.st_mode)) {
            s.Categories = (struct category*)realloc(s.Categories, sizeof(struct category) * (s.CategoryCount + 1));
            s.Categories[s.CategoryCount] = traverse_category(subdirpath);
            s.CategoryCount++;
        }
    }

    closedir(dp);
    return s;
}

// Function to get all stores
void get_stores(const char *root_path, struct store **stores, int *store_count) {
    struct dirent *entry;
    DIR *dp = opendir(root_path);
    if (!dp) {
        perror("Error opening root directory");
        exit(EXIT_FAILURE);
    }

    *store_count = 0;
    *stores = NULL;

    while ((entry = readdir(dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char store_path[1024];
        snprintf(store_path, sizeof(store_path), "%s/%s", root_path, entry->d_name);

        struct stat path_stat;
        stat(store_path, &path_stat);

        if (S_ISDIR(path_stat.st_mode)) {
            *stores = (struct store*)realloc(*stores, sizeof(struct store) * (*store_count + 1));
            (*stores)[*store_count] = traverse_store(store_path);
            (*store_count)++;
        }
    }

    closedir(dp);
}

#endif  // DATASET_LOADER_H