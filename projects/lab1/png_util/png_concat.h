#pragma once

#include <stddef.h>

int png_concat_write(const char *path, const unsigned char *ihdr_data,
                     const unsigned char *const *scanlines,
                     const unsigned int *scanline_lengths, size_t count);