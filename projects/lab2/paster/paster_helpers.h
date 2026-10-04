#ifndef PASTER_HELPERS_H
#define PASTER_HELPERS_H

#include <stddef.h>
#include <pthread.h>

#define PASTER_HEADER "X-Ece252-Fragment: "
#define PASTER_RECV_BUF_INIT_SIZE 16384
#define PASTER_RECV_BUF_INC 8192

typedef struct {
    char *buf;
    size_t size;
    size_t max_size;
    int seq;
} paster_recv_buf;

int paster_recv_buf_init(paster_recv_buf *buf, size_t capacity);
void paster_recv_buf_cleanup(paster_recv_buf *buf);
size_t paster_header_callback(char *received, size_t size, size_t nmemb,
                              void *userdata);
size_t paster_write_callback(char *received, size_t size, size_t nmemb,
                             void *userdata);
int paster_parse_options(int argc, char **argv, int *num_threads, int *img_num);
int paster_run_threads(pthread_t *threads, size_t count,
                       void *(*worker)(void *), void *worker_args,
                       size_t worker_arg_size, size_t *created);

#endif
