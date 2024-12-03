#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// Function to read the contents of a file
void read_file(const char *filepath) {
    printf("[DEBUG] Attempting to read file: %s\n", filepath);

    FILE *file = fopen(filepath, "r");
    if (file == NULL) {
        perror("[ERROR] Error opening file");
        return;
    }

    printf("[DEBUG] Successfully opened file: %s\n", filepath);
    char buffer[1024];
    printf("[DEBUG] File contents:\n");
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        printf("%s", buffer);
    }
    printf("\n[DEBUG] Finished reading file: %s\n\n", filepath);
    fclose(file);
}

// Function to traverse directories and process files
void traverse_directory(const char *dirpath) {
    printf("[DEBUG] Opening directory: %s\n", dirpath);

    struct dirent *entry;
    DIR *dp = opendir(dirpath);

    if (dp == NULL) {
        perror("[ERROR] Error opening directory");
        return;
    }

    while ((entry = readdir(dp)) != NULL) {
        // Skip "." and ".." directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Build the full path of the entry
        char newpath[1024];
        snprintf(newpath, sizeof(newpath), "%s/%s", dirpath, entry->d_name);

        // Use stat to determine if it is a file or directory
        struct stat path_stat;
        if (stat(newpath, &path_stat) == -1) {
            perror("[ERROR] Error getting file status");
            continue;
        }

        if (S_ISDIR(path_stat.st_mode)) {
            // Entry is a directory, recurse into it
            printf("[DEBUG] Found directory: %s\n", newpath);
            traverse_directory(newpath);
        } else if (S_ISREG(path_stat.st_mode)) {
            // Entry is a regular file, process it
            printf("[DEBUG] Found file: %s\n", newpath);
            read_file(newpath);
        }
    }

    closedir(dp);
    printf("[DEBUG] Finished processing directory: %s\n", dirpath);
}

int main() {
    const char *root_dir = "E:/Operating System/project/OS_PROJECT/Data_set"; // Adjust the path to your dataset
    printf("[DEBUG] Starting directory traversal from root: %s\n", root_dir);
    traverse_directory(root_dir);
    printf("[DEBUG] Finished directory traversal.\n");
    return 0;
}
