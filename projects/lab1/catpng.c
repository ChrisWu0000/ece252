#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zutil.h"  /* for mem_def(), mem_inf(), compressBound() */
#include "png_concat.h"

typedef unsigned char U8;
typedef unsigned int  U32;

#define PNG_SIG_SIZE   8
#define IHDR_DATA_SIZE 13

static const U8 PNG_SIGNATURE[PNG_SIG_SIZE] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a
};

static U32 read_be32(const U8 *buf)
{
    return ((U32)buf[0] << 24) | ((U32)buf[1] << 16) |
           ((U32)buf[2] << 8)  | (U32)buf[3];
}

static void write_be32(U8 *buf, U32 value)
{
    buf[0] = (U8)(value >> 24);
    buf[1] = (U8)(value >> 16);
    buf[2] = (U8)(value >> 8);
    buf[3] = (U8)value;
}

/* IHDR fields and inflated (filtered) scanline data of one input PNG strip. */
typedef struct {
    U32 width;
    U32 height;
    U8  bit_depth;
    U8  color_type;
    U8  compression;
    U8  filter;
    U8  interlace;
    U8  *scanlines;
    U32 scanlines_len;
} png_strip;

/* Read a PNG file, capture its IHDR fields, and inflate its IDAT data into
 * out->scanlines. Assumes exactly one IHDR chunk and one IDAT chunk, per the
 * lab's simplified PNG format. */
static int read_png_strip(const char *path, png_strip *out)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "catpng: cannot open %s\n", path);
        return 1;
    }

    U8 sig[PNG_SIG_SIZE];
    if (fread(sig, 1, PNG_SIG_SIZE, fp) != PNG_SIG_SIZE ||
        memcmp(sig, PNG_SIGNATURE, PNG_SIG_SIZE) != 0) {
        fprintf(stderr, "catpng: %s is not a PNG file\n", path);
        fclose(fp);
        return 1;
    }

    int have_ihdr = 0;
    U8 *idat_data = NULL;
    U32 idat_len = 0;

    for (;;) {
        U8 len_type[8];
        if (fread(len_type, 1, 8, fp) != 8) {
            break;
        }
        U32 data_len = read_be32(len_type);
        U8 *chunk_type = len_type + 4;

        U8 *data = NULL;
        if (data_len > 0) {
            data = malloc(data_len);
            if (!data || fread(data, 1, data_len, fp) != data_len) {
                fprintf(stderr, "catpng: failed to read a chunk in %s\n", path);
                free(data);
                free(idat_data);
                fclose(fp);
                return 1;
            }
        }
        fseek(fp, 4, SEEK_CUR); /* skip CRC field, not validated here */

        if (memcmp(chunk_type, "IHDR", 4) == 0 && data_len == IHDR_DATA_SIZE) {
            out->width       = read_be32(data);
            out->height      = read_be32(data + 4);
            out->bit_depth   = data[8];
            out->color_type  = data[9];
            out->compression = data[10];
            out->filter      = data[11];
            out->interlace   = data[12];
            have_ihdr = 1;
        } else if (memcmp(chunk_type, "IDAT", 4) == 0 && idat_data == NULL) {
            idat_data = data;
            idat_len = data_len;
            data = NULL; /* ownership moved to idat_data */
        } else if (memcmp(chunk_type, "IEND", 4) == 0) {
            free(data);
            break;
        }
        free(data);
    }
    fclose(fp);

    if (!have_ihdr || idat_data == NULL) {
        fprintf(stderr, "catpng: %s missing IHDR or IDAT chunk\n", path);
        free(idat_data);
        return 1;
    }

    U32 rowbytes = 1 + out->width * 4; /* filter byte + RGBA pixels */
    U32 cap = rowbytes * out->height;
    U8 *inflated = malloc(cap);
    if (!inflated) {
        fprintf(stderr, "catpng: out of memory inflating %s\n", path);
        free(idat_data);
        return 1;
    }

    U64 inf_len = 0;
    int ret = mem_inf(inflated, &inf_len, idat_data, idat_len);
    free(idat_data);
    if (ret != 0) {
        fprintf(stderr, "catpng: failed to inflate IDAT data in %s\n", path);
        free(inflated);
        return 1;
    }

    out->scanlines = inflated;
    out->scanlines_len = (U32)inf_len;
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s PNG_FILE...\n", argv[0]);
        return 1;
    }

    int n = argc - 1;
    png_strip *strips = calloc((size_t)n, sizeof(png_strip));
    if (!strips) {
        fprintf(stderr, "catpng: out of memory\n");
        return 1;
    }

    U32 total_height = 0;
    U64 total_scanlines_len = 0;

    for (int i = 0; i < n; i++) {
        if (read_png_strip(argv[i + 1], &strips[i]) != 0) {
            for (int j = 0; j < i; j++) free(strips[j].scanlines);
            free(strips);
            return 1;
        }
        if (strips[i].width != strips[0].width) {
            fprintf(stderr, "catpng: %s width does not match %s\n", argv[i + 1], argv[1]);
            for (int j = 0; j <= i; j++) free(strips[j].scanlines);
            free(strips);
            return 1;
        }
        total_height += strips[i].height;
        total_scanlines_len += strips[i].scanlines_len;
    }

    U8 ihdr_data[IHDR_DATA_SIZE];
    write_be32(ihdr_data, strips[0].width);
    write_be32(ihdr_data + 4, total_height);
    ihdr_data[8]  = strips[0].bit_depth;
    ihdr_data[9]  = strips[0].color_type;
    ihdr_data[10] = strips[0].compression;
    ihdr_data[11] = strips[0].filter;
    ihdr_data[12] = strips[0].interlace;

    const U8 **scanlines = malloc((size_t)n * sizeof(*scanlines));
    U32 *lengths = malloc((size_t)n * sizeof(*lengths));
    if (!scanlines || !lengths) {
        for (int i = 0; i < n; i++) free(strips[i].scanlines);
        free(scanlines);
        free(lengths);
        free(strips);
        fprintf(stderr, "catpng: out of memory\n");
        return 1;
    }
    for (int i = 0; i < n; i++) {
        scanlines[i] = strips[i].scanlines;
        lengths[i] = strips[i].scanlines_len;
    }
    int ok = png_concat_write("all.png", ihdr_data, scanlines, lengths, (size_t)n);
    for (int i = 0; i < n; i++) free(strips[i].scanlines);
    free(scanlines);
    free(lengths);
    free(strips);

    if (!ok) {
        fprintf(stderr, "catpng: failed to write all.png\n");
        return 1;
    }

    return 0;
}
