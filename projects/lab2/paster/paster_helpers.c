#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "paster_helpers.h"

/* Receive-buffer and callback pattern adapted from the lab2 cURL starter. */
int paster_recv_buf_init(paster_recv_buf *buf, size_t capacity)
{
    if (buf == NULL || capacity == 0) return 1;
    buf->buf = malloc(capacity);
    if (buf->buf == NULL) return 1;
    buf->size = 0;
    buf->max_size = capacity;
    buf->seq = -1;
    return 0;
}

void paster_recv_buf_cleanup(paster_recv_buf *buf)
{
    if (buf == NULL) return;
    free(buf->buf);
    buf->buf = NULL;
    buf->size = 0;
    buf->max_size = 0;
}

size_t paster_header_callback(char *received, size_t size, size_t nmemb,
                              void *userdata)
{
    size_t length = size * nmemb;
    paster_recv_buf *buf = userdata;
    const size_t prefix_length = sizeof(PASTER_HEADER) - 1;

    if (length <= prefix_length ||
        memcmp(received, PASTER_HEADER, prefix_length) != 0) {
        return length;
    }

    const char *p = received + prefix_length;
    const char *end = received + length;
    int seq = 0;
    int digits = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        int digit = *p - '0';
        if (seq > (INT_MAX - digit) / 10) return length;
        seq = seq * 10 + digit;
        digits++;
        p++;
    }
    if (digits > 0 && (p == end || *p == '\r' || *p == '\n')) {
        buf->seq = seq;
    }
    return length;
}

size_t paster_write_callback(char *received, size_t size, size_t nmemb,
                             void *userdata)
{
    paster_recv_buf *buf = userdata;
    if (size != 0 && nmemb > (size_t)-1 / size) return 0;
    size_t received_length = size * nmemb;
    if (buf->size == (size_t)-1 ||
        received_length > (size_t)-1 - buf->size - 1) return 0;

    size_t required = buf->size + received_length + 1;
    if (required > buf->max_size) {
        size_t increment = received_length + 1 > PASTER_RECV_BUF_INC
                               ? received_length + 1
                               : PASTER_RECV_BUF_INC;
        if (increment > (size_t)-1 - buf->max_size) return 0;
        size_t new_size = buf->max_size + increment;
        char *grown = realloc(buf->buf, new_size);
        if (grown == NULL) return 0;
        buf->buf = grown;
        buf->max_size = new_size;
    }

    memcpy(buf->buf + buf->size, received, received_length);
    buf->size += received_length;
    buf->buf[buf->size] = '\0';
    return received_length;
}

static int parse_option_value(const char *value, int minimum, int maximum,
                               int *result)
{
    if (*value == '=') value++;
    if (*value == '\0') return 0;

    errno = 0;
    char *end;
    long parsed = strtol(value, &end, 10);
    if (errno == ERANGE || end == value || *end != '\0' ||
        parsed < minimum || parsed > maximum) return 0;
    *result = (int)parsed;
    return 1;
}

/* Option parsing adapts the -t/-n getopt example in lab2/getopt/main_getopt.c. */
int paster_parse_options(int argc, char **argv, int *num_threads, int *img_num)
{
    *num_threads = 1;
    *img_num = 1;
    int option;
    while ((option = getopt(argc, argv, "t:n:")) != -1) {
        if (option == 't') {
            if (!parse_option_value(optarg, 1, INT_MAX, num_threads)) {
                fprintf(stderr, "%s: option requires an argument > 0 -- 't'\n", argv[0]);
                return 0;
            }
        } else if (option == 'n') {
            if (!parse_option_value(optarg, 1, 3, img_num)) {
                fprintf(stderr, "%s: option requires 1, 2, or 3 -- 'n'\n", argv[0]);
                return 0;
            }
        } else {
            fprintf(stderr, "Usage: %s [-t NUM] [-n NUM]\n", argv[0]);
            return 0;
        }
    }
    if (optind != argc) {
        fprintf(stderr, "Usage: %s [-t NUM] [-n NUM]\n", argv[0]);
        return 0;
    }
    return 1;
}

/* Create/join pattern adapts lab2/pthreads/main.c; worker arguments stay caller-owned. */
int paster_run_threads(pthread_t *threads, size_t count,
                       void *(*worker)(void *), void *worker_args,
                       size_t worker_arg_size, size_t *created)
{
    if (threads == NULL || worker == NULL || worker_args == NULL ||
        worker_arg_size == 0 || created == NULL) return EINVAL;
    *created = 0;
    int result = 0;
    for (size_t i = 0; i < count; i++) {
        void *arg = (unsigned char *)worker_args + i * worker_arg_size;
        result = pthread_create(&threads[i], NULL, worker, arg);
        if (result != 0) break;
        (*created)++;
    }
    int create_result = result;
    for (size_t i = 0; i < *created; i++) {
        int join_result = pthread_join(threads[i], NULL);
        if (join_result != 0 && result == 0) result = join_result;
    }
    return create_result != 0 ? create_result : result;
}
