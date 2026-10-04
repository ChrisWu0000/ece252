#include <stdlib.h>
#include <string.h>

#include "png_concat.h"
#include "zutil.h"
#include "crc.h"

typedef unsigned int U32;

static void write_be32(U8 *buf, U32 value)
{
    buf[0] = (U8)(value >> 24);
    buf[1] = (U8)(value >> 16);
    buf[2] = (U8)(value >> 8);
    buf[3] = (U8)value;
}

static int write_chunk(FILE *fp, const char *type, const U8 *data, U32 len)
{
    U8 length[4], checksum[4];
    U8 *crc_data = malloc(4 + (size_t)len);
    if (!crc_data) return 0;

    write_be32(length, len);
    memcpy(crc_data, type, 4);
    if (len) memcpy(crc_data + 4, data, len);
    write_be32(checksum, (U32)crc(crc_data, (int)(4 + len)));
    free(crc_data);

    return fwrite(length, 1, 4, fp) == 4 && fwrite(type, 1, 4, fp) == 4 &&
           (!len || fwrite(data, 1, len, fp) == len) &&
           fwrite(checksum, 1, 4, fp) == 4;
}

int png_concat_write(const char *path, const U8 *ihdr_data,
                     const U8 *const *scanlines, const U32 *scanline_lengths,
                     size_t count)
{
    if (!path || !ihdr_data || !scanlines || !scanline_lengths || count == 0) {
        return 0;
    }

    U64 total_len = 0;
    for (size_t i = 0; i < count; i++) {
        if (!scanlines[i] || total_len + scanline_lengths[i] < total_len) {
            return 0;
        }
        total_len += scanline_lengths[i];
    }

    U8 *joined = malloc((size_t)total_len);
    if (!joined) return 0;
    U8 *next = joined;
    for (size_t i = 0; i < count; i++) {
        memcpy(next, scanlines[i], scanline_lengths[i]);
        next += scanline_lengths[i];
    }

    U64 compressed_cap = compressBound(total_len);
    U8 *compressed = malloc((size_t)compressed_cap);
    if (!compressed) {
        free(joined);
        return 0;
    }

    U64 compressed_len = 0;
    int result = mem_def(compressed, &compressed_len, joined, total_len,
                         Z_DEFAULT_COMPRESSION);
    free(joined);
    if (result != Z_OK || compressed_len > 0xffffffffUL) {
        free(compressed);
        return 0;
    }

    static const U8 signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(compressed);
        return 0;
    }
    result = fwrite(signature, 1, sizeof(signature), fp) == sizeof(signature) &&
             write_chunk(fp, "IHDR", ihdr_data, 13) &&
             write_chunk(fp, "IDAT", compressed, (U32)compressed_len) &&
             write_chunk(fp, "IEND", NULL, 0);
    if (fclose(fp) != 0) result = 0;
    free(compressed);
    return result;
}