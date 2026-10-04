#define _DEFAULT_SOURCE

#include <stdio.h>   /* for printf(), perror()...   */
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define PNG_SIG_SIZE 8
typedef unsigned char U8;

static int is_png(const U8 *buf, size_t n)
{
    static const U8 signature[PNG_SIG_SIZE] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a
    };

    return buf != NULL && n >= PNG_SIG_SIZE &&
           memcmp(buf, signature, PNG_SIG_SIZE) == 0;
}


int find_pngs(const char *dir_path, int *count);

/* Recursively search a directory and print the paths of valid PNG files. */
int find_pngs(const char *dir_path, int *count) {
    /* Open the directory so its entries can be inspected. */
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("opendir failed");
        return 1;
    }

    struct dirent *entry;
    char path[1024];

    /* Process each file or subdirectory in the current directory. */
    while ((entry = readdir(dir)) != NULL) {
        // Skip current and parent directory entries to prevent infinite recursion
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        struct stat statbuf;
        // Avoid following symlinks on platforms that provide lstat.
    #ifdef _WIN32
        if (stat(path, &statbuf) == -1) {
    #else
        if (lstat(path, &statbuf) == -1) {
    #endif
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            /* Search subdirectories recursively. */
            find_pngs(path, count);
        } else if (S_ISREG(statbuf.st_mode)) {
            /* Read the signature and reuse is_png to identify PNG files. */
            FILE *fp = fopen(path, "rb");
            U8 signature[PNG_SIG_SIZE];

            if (fp != NULL &&
                fread(signature, 1, PNG_SIG_SIZE, fp) == PNG_SIG_SIZE &&
                is_png(signature, PNG_SIG_SIZE)) {
                printf("%s\n", path);
                (*count)++;
            }
            if (fp != NULL) {
                fclose(fp);
            }
        }
    }
    closedir(dir);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s DIRECTORY\n", argv[0]);
        return 1;
    }

    const char *dir = argv[1];
    int count = 0;
    
    find_pngs(dir, &count);
    
    if (count == 0) {
        printf("findpng: No PNG file found\n");
    }
    
    return 0;
}