#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// Function to read the contents of a file
void read_file(const char *filepath)
{
    FILE *file = fopen(filepath, "r");
    if (file == NULL)
    {
        perror("Error opening file");
        return;
    }

    char buffer[1024];
    printf("Contents of %s:\n", filepath);
    while (fgets(buffer, sizeof(buffer), file) != NULL)
    {
        printf("%s", buffer);
    }
    printf("\n");
    fclose(file);
}

// Function to traverse directories and process files
void traverse_directory(const char *dirpath)
{
    struct dirent *entry;
    DIR *dp = opendir(dirpath);

    if (dp == NULL)
    {
        perror("Error opening directory");
        return;
    }

    while ((entry = readdir(dp)) != NULL)
    {
        // Skip "." and ".." directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Build the full path of the entry
        char newpath[1024];
        snprintf(newpath, sizeof(newpath), "%s/%s", dirpath, entry->d_name);

        // Use stat to determine if it is a file or directory
        struct stat path_stat;
        if (stat(newpath, &path_stat) == -1)
        {
            perror("Error getting file status");
            continue;
        }

        if (S_ISDIR(path_stat.st_mode))
        {
            // Entry is a directory, recurse into it
            traverse_directory(newpath);
        }
        else if (S_ISREG(path_stat.st_mode))
        {
            // Entry is a regular file, process it
            read_file(newpath);
        }
    }

    closedir(dp);
}

int main() {
    const char *root_dir = "E:/Operating System/project/OS_PROJECT/Data_set"; // Use forward slashes
    traverse_directory(root_dir);
    return 0;
}

