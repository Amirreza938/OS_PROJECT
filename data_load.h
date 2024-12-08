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

// Function to read a file and parse its content into a product
struct product read_file(const char *filepath) {
    struct product p = {0};
    FILE *file = fopen(filepath, "r");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
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