/**
 * Download the 50 strips for an ECE252 picture concurrently and write all.png.
 */

#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include <curl/curl.h>
#include "crc.h"
#include "lab_png.h"
#include "zutil.h"

#define PASTER_RECV_BUF_INIT_SIZE 16384

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

#define NUM_STRIPS 50
#define NUM_SERVERS 3
#define MAX_FAILS 5

static const char *SERVERS[NUM_SERVERS] = {
    "ece252-1.uwaterloo.ca",
    "ece252-2.uwaterloo.ca",
    "ece252-3.uwaterloo.ca"
};

static const U8 PNG_SIG[PNG_SIG_SIZE] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a
};

typedef struct strips {
    U8 *png[NUM_STRIPS];
    size_t len[NUM_STRIPS];
    int count;
    pthread_mutex_t lock;
} STRIPS;

typedef struct thread_args {
    int id;
    int img;
    STRIPS *shared;
} THREAD_ARGS;

static int all_received(STRIPS *strips)
{
    pthread_mutex_lock(&strips->lock);
    int done = strips->count == NUM_STRIPS;
    pthread_mutex_unlock(&strips->lock);
    return done;
}

/* Worker entry-point pattern adapted from the lab2 pthread starter. */
static void *fetch_strips(void *arg)
{
    THREAD_ARGS *args = arg;
    STRIPS *strips = args->shared;
    char url[128];
    snprintf(url, sizeof(url), "http://%s:2520/image?img=%d",
             SERVERS[args->id % NUM_SERVERS], args->img);

    paster_recv_buf response;
    if (paster_recv_buf_init(&response, PASTER_RECV_BUF_INIT_SIZE) != 0) return NULL;

    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        paster_recv_buf_cleanup(&response);
        return NULL;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, paster_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, paster_header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    int failures = 0;
    while (!all_received(strips) && failures < MAX_FAILS) {
        response.size = 0;
        response.seq = -1;
        CURLcode result = curl_easy_perform(curl);
        if (result != CURLE_OK) {
            failures++;
            continue;
        }
        failures = 0;
        if (response.seq < 0 || response.seq >= NUM_STRIPS) continue;

        pthread_mutex_lock(&strips->lock);
        if (strips->png[response.seq] == NULL) {
            U8 *copy = malloc(response.size);
            if (copy != NULL) {
                memcpy(copy, response.buf, response.size);
                strips->png[response.seq] = copy;
                strips->len[response.seq] = response.size;
                strips->count++;
            }
        }
        pthread_mutex_unlock(&strips->lock);
    }

    curl_easy_cleanup(curl);
    paster_recv_buf_cleanup(&response);
    return NULL;
}

static int paste_strips(STRIPS *strips, const char *path)
{
    U8 *ihdr[NUM_STRIPS];
    U8 *idat[NUM_STRIPS];
    U32 idat_len[NUM_STRIPS];

    U32 width = 0;
    U32 total_height = 0;

    for (int i = 0; i < NUM_STRIPS; i++) {
        U8 *png = strips->png[i];
        size_t png_len = strips->len[i];
        if (png == NULL || png_len < PNG_SIG_SIZE + 25 ||
            memcmp(png, PNG_SIG, PNG_SIG_SIZE) != 0) {
            printf("strip %d is not a valid PNG\n", i);
            return -1;
        }

        U32 ihdr_len;
        memcpy(&ihdr_len, png + PNG_SIG_SIZE, sizeof(ihdr_len));
        ihdr_len = ntohl(ihdr_len);
        if (ihdr_len != DATA_IHDR_SIZE ||
            memcmp(png + PNG_SIG_SIZE + 4, "IHDR", 4) != 0) {
            printf("strip %d has an invalid IHDR chunk\n", i);
            return -1;
        }
        ihdr[i] = png + PNG_SIG_SIZE + 8;

        size_t idat_pos = PNG_SIG_SIZE + 12 + (size_t)ihdr_len;
        if (idat_pos > png_len || png_len - idat_pos < 24 ||
            memcmp(png + idat_pos + 4, "IDAT", 4) != 0) {
            printf("strip %d has an invalid IDAT chunk\n", i);
            return -1;
        }
        memcpy(&idat_len[i], png + idat_pos, sizeof(idat_len[i]));
        idat_len[i] = ntohl(idat_len[i]);
        if ((size_t)idat_len[i] > png_len - idat_pos - 24) {
            printf("strip %d has truncated IDAT data\n", i);
            return -1;
        }
        idat[i] = png + idat_pos + 8;

        size_t iend_pos = idat_pos + 12 + (size_t)idat_len[i];
        U32 iend_len;
        memcpy(&iend_len, png + iend_pos, sizeof(iend_len));
        if (ntohl(iend_len) != 0 ||
            memcmp(png + iend_pos + 4, "IEND", 4) != 0) {
            printf("strip %d has an invalid IEND chunk\n", i);
            return -1;
        }

        U32 strip_width, strip_height;
        memcpy(&strip_width, ihdr[i], sizeof(strip_width));
        memcpy(&strip_height, ihdr[i] + 4, sizeof(strip_height));
        strip_width = ntohl(strip_width);
        strip_height = ntohl(strip_height);
        if (i == 0) {
            width = strip_width;
        } else if (strip_width != width) {
            printf("strip %d: width %u does not match %u\n", i, strip_width, width);
            return -1;
        }
        total_height += strip_height;
    }

    U64 raw_len = (U64)total_height * (1 + (U64)width * 4);
    U8 *raw = malloc(raw_len);

    U64 offset = 0;
    for (int i = 0; i < NUM_STRIPS; i++) {
        U64 inflated_len = 0;
        mem_inf(raw + offset, &inflated_len, idat[i], idat_len[i]);
        offset += inflated_len;
    }

    U8 *compressed = malloc(compressBound(raw_len));
    U64 compressed_len = 0;
    mem_def(compressed, &compressed_len, raw, raw_len, Z_DEFAULT_COMPRESSION);

    U8 new_ihdr[DATA_IHDR_SIZE];
    memcpy(new_ihdr, strips->png[0] + PNG_SIG_SIZE + 8, DATA_IHDR_SIZE);
    U32 height_network = htonl(total_height);
    memcpy(new_ihdr + 4, &height_network, sizeof(height_network));

    FILE *out = fopen(path, "wb");
    fwrite(PNG_SIG, 1, PNG_SIG_SIZE, out);

    const char *types[] = { "IHDR", "IDAT", "IEND" };
    U8 *data[] = { new_ihdr, compressed, NULL };
    U32 lengths[] = { DATA_IHDR_SIZE, (U32)compressed_len, 0 };
    for (int i = 0; i < 3; i++) {
        struct chunk chunk = {0};
        chunk.length = lengths[i];
        memcpy(chunk.type, types[i], CHUNK_TYPE_SIZE);
        chunk.p_data = data[i];

        U32 length_network = htonl(chunk.length);
        U32 crc_network = htonl(calculate_chunk_crc(&chunk));
        fwrite(&length_network, 1, CHUNK_LEN_SIZE, out);
        fwrite(chunk.type, 1, CHUNK_TYPE_SIZE, out);
        if (chunk.length != 0) fwrite(chunk.p_data, 1, chunk.length, out);
        fwrite(&crc_network, 1, CHUNK_CRC_SIZE, out);
    }
    fclose(out);
    free(raw);
    free(compressed);
    return 0;
}

int main(int argc, char *argv[])
{
    int num_threads = 1;
    int img = 1;
    int option;
    /* getopt/strtoul option handling follows lab2/getopt/main_getopt.c. */
    while ((option = getopt(argc, argv, "t:n:")) != -1) {
        if (option == 't') {
            const char *value = optarg[0] == '=' ? optarg + 1 : optarg;
            unsigned long parsed = strtoul(value, NULL, 10);
            if (parsed == 0 || parsed > INT_MAX) return 1;
            num_threads = (int)parsed;
        } else if (option == 'n') {
            const char *value = optarg[0] == '=' ? optarg + 1 : optarg;
            unsigned long parsed = strtoul(value, NULL, 10);
            if (parsed < 1 || parsed > 3) return 1;
            img = (int)parsed;
        } else {
            printf("Usage: %s [-t NUM] [-n NUM]\n", argv[0]);
            return 1;
        }
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    STRIPS strips = {0};
    pthread_mutex_init(&strips.lock, NULL);

    pthread_t *threads = malloc((size_t)num_threads * sizeof(*threads));
    THREAD_ARGS *args = malloc((size_t)num_threads * sizeof(*args));
    int started = 0;
    for (int i = 0; i < num_threads; i++) {
        args[i].id = i;
        args[i].img = img;
        args[i].shared = &strips;
        if (pthread_create(&threads[i], NULL, fetch_strips, &args[i]) != 0) break;
        started++;
    }
    for (int i = 0; i < started; i++) pthread_join(threads[i], NULL);

    int status = 0;
    if (strips.count == NUM_STRIPS) {
        status = paste_strips(&strips, "all.png") != 0;
    } else {
        printf("only received %d of %d strips\n", strips.count, NUM_STRIPS);
        status = 1;
    }

    for (int i = 0; i < NUM_STRIPS; i++) free(strips.png[i]);
    free(threads);
    free(args);
    pthread_mutex_destroy(&strips.lock);
    curl_global_cleanup();
    return status;
}