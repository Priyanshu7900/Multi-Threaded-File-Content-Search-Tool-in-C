// c_fastfind.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_THREADS 8
#define MAX_PATH 1024
#define MAX_LINE 1024

typedef struct {
    char filepath[MAX_PATH];
    char keyword[128];
} thread_arg_t;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *search_in_file(void *arg) {
    thread_arg_t *data = (thread_arg_t *)arg;
    FILE *file = fopen(data->filepath, "r");
    if (!file) {
        pthread_mutex_lock(&lock);
        fprintf(stderr, "[Error] Cannot open file: %s\n", data->filepath);
        pthread_mutex_unlock(&lock);
        free(arg);
        pthread_exit(NULL);
    }

    char line[MAX_LINE];
    int line_num = 0;
    while (fgets(line, sizeof(line), file)) {
        line_num++;
        if (strstr(line, data->keyword)) {
            pthread_mutex_lock(&lock);
            printf("[Match] %s:%d: %s", data->filepath, line_num, line);
            pthread_mutex_unlock(&lock);
        }
    }

    fclose(file);
    free(arg);
    pthread_exit(NULL);
}

void search_directory(const char *dirpath, const char *keyword) {
    DIR *dir = opendir(dirpath);
    if (!dir) {
        fprintf(stderr, "[Error] Cannot open directory: %s\n", dirpath);
        return;
    }

    struct dirent *entry;
    pthread_t threads[MAX_THREADS];
    int thread_count = 0;

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char fullpath[MAX_PATH];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);

        struct stat statbuf;
        if (stat(fullpath, &statbuf) == -1) continue;

        if (S_ISDIR(statbuf.st_mode)) {
            search_directory(fullpath, keyword);
        } else if (S_ISREG(statbuf.st_mode)) {
            if (thread_count >= MAX_THREADS) {
                for (int i = 0; i < thread_count; ++i)
                    pthread_join(threads[i], NULL);
                thread_count = 0;
            }

            thread_arg_t *arg = malloc(sizeof(thread_arg_t));
            strncpy(arg->filepath, fullpath, MAX_PATH);
            strncpy(arg->keyword, keyword, sizeof(arg->keyword));

            pthread_create(&threads[thread_count++], NULL, search_in_file, arg);
        }
    }

    for (int i = 0; i < thread_count; ++i)
        pthread_join(threads[i], NULL);

    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <directory> <keyword>\n", argv[0]);
        return 1;
    }

    const char *directory = argv[1];
    const char *keyword = argv[2];

    printf("[Search] Searching for '%s' in %s...\n", keyword, directory);
    search_directory(directory, keyword);
    printf("[Done] Search complete.\n");

    return 0;
}
