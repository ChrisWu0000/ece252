#include <stdio.h>
#include <stdlib.h>  // For rand(), srand(), atoi(), and exit()

#define NUM_VALUES 50

int main(int argc, char *argv[]) {
    // Check for seed argument
    if (argc < 2) {
        fprintf(stderr, "Error: Seed argument missing. Usage: %s <seed>\n", argv[0]);
        return 1;  // Exit with nonzero status
    }

    // Parse seed from command line using atoi
    int seed = atoi(argv[1]);
    if (seed < 0) {
        fprintf(stderr, "Error: Invalid seed '%s'. Must be a non-negative integer.\n", argv[1]);
        return 1;
    }

    // Seed the random number generator
    srand((unsigned int)seed);

    printf("Generated 100 fixed random numbers (same every run with seed %d):\n", seed);
    for (int i = 0; i < NUM_VALUES; i++) {
        int random_num = rand() % 100;  // Generate number between 0 and 99 (adjust range as needed)
        printf("%d ", random_num);
        if ((i + 1) % 10 == 0) printf("\n");  // Line break every 10 numbers for readability
    }
    printf("\n");

    return 0;
}
