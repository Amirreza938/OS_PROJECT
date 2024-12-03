#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Structure to hold product data
struct product {
    char *Name;
    float Price;
    float Score;
    int Entity;
    char LastModified[64]; // Field to store last modified date and time
};

// Function to read a file and parse its content into a struct product
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

// Function to free memory allocated for a product
void free_product(struct product *p) {
    if (p->Name != NULL) {
        free(p->Name);
        p->Name = NULL;
    }
}

int main() {
    const char *file_path = "E:/Operating System/project/OS_PROJECT/Data_set/Store1/Sports/649.txt"; // Replace with actual file path

    struct product p = read_file(file_path);

    printf("Product Details:\n");
    printf("Name: %s\n", p.Name);
    printf("Price: %.2f\n", p.Price);
    printf("Score: %.2f\n", p.Score);
    printf("Entity: %d\n", p.Entity);
    printf("Last Modified: %s\n", p.LastModified);

    free_product(&p); // Free allocated memory for product name
    return 0;
}