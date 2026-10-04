/**
 * @brief  micros and structures for a simple PNG file 
 *
 * Copyright 2018-2020 Yiqing Huang
 * Updated 2024 m27ma
 *
 * This software may be freely redistributed under the terms of MIT License
 */
#pragma once

/******************************************************************************
 * INCLUDE HEADER FILES
 *****************************************************************************/
#include <stdio.h>

/******************************************************************************
 * DEFINED MACROS 
 *****************************************************************************/

//note: these sizes are for accessing PNG files, not for accessing data structure types (might be padded)
#define PNG_SIG_SIZE    8 /* number of bytes of png image signature data */
#define CHUNK_LEN_SIZE  4 /* chunk length field size in bytes */          
#define CHUNK_TYPE_SIZE 4 /* chunk type field size in bytes */
#define CHUNK_CRC_SIZE  4 /* chunk CRC field size in bytes */
#define DATA_IHDR_SIZE 13 /* IHDR chunk data field size */

/******************************************************************************
 * STRUCTURES and TYPEDEFS 
 *****************************************************************************/
typedef unsigned char U8;
typedef unsigned int  U32;

typedef struct chunk {
    U32 length;  /* length of data in the chunk, host byte order */
    U8  type[4]; /* chunk type */
    U8  *p_data; /* pointer to location where the actual data are */
    U32 crc;     /* CRC field  */
} *chunk_p;

/* note that there are 13 Bytes valid data, compiler will padd 3 bytes to make
   the structure 16 Bytes due to alignment. So do not use the size of this
   structure if you need the actual data size, use 13 Bytes (i.e DATA_IHDR_SIZE macro).
 */
typedef struct data_IHDR {// IHDR chunk data field
    U32 width;        /* width in pixels, big endian   */
    U32 height;       /* height in pixels, big endian  */
    U8  bit_depth;    /* num of bits per sample or per palette index.
                         valid values are: 1, 2, 4, 8, 16 */
    U8  color_type;   /* =0: Grayscale; =2: Truecolor; =3 Indexed-color
                         =4: Greyscale with alpha; =6: Truecolor with alpha */
    U8  compression;  /* only method 0 is defined for now */
    U8  filter;       /* only method 0 is defined for now */
    U8  interlace;    /* =0: no interlace; =1: Adam7 interlace */
} *data_IHDR_p;

/* A simple PNG file format, three chunks only*/
typedef struct simple_PNG {
    struct chunk *p_IHDR;
    struct chunk *p_IDAT;  /* only handles one IDAT chunk */  
    struct chunk *p_IEND;
} *simple_PNG_p;

/******************************************************************************
 * FUNCTION PROTOTYPES 
 *****************************************************************************/
/* this is one possible way to structure the PNG manipulation functions */

int is_png(U8 *buf, size_t n); //check if PNG signature is present
    //takes in pointer to least 8 bytes of binary data
    


int get_png_data_IHDR(struct data_IHDR *out, FILE *fp, long offset, int whence); //extract from file the data field of the IHDR chunk, to populate a struct data_IHDR
    //takes in file pointer and how to reach data field of the IDHR chunk (see fseek parameters)
int get_png_height(struct data_IHDR *buf); //read out image height from a struct data_IHDR
int get_png_width(struct data_IHDR *buf); //read out image width from a struct data_IHDR

int get_png_chunks(simple_PNG_p out, FILE* fp, long offset, int whence); //extract from file all chunks in a png, to populate a struct simple_PNG
    //takes in file pointer and how to reach the IHDR chunk (see fseek parameters)
chunk_p get_chunk(FILE *fp); //extract from file one chunk and populate a struct chunk
    //takes in file pointer that is already at the start of the target chunk

U32 get_chunk_crc(chunk_p in); //read out expected crc from a struct chunk
U32 calculate_chunk_crc(chunk_p in); //calculate crc using chunk type and chunk data

simple_PNG_p mallocPNG(); //allocate memory for a struct simple_PNG
void free_png( simple_PNG_p in); //free the memory of a struct simple_PNG
void free_chunk(chunk_p in); //free the memory of a struct chunk and inner data buffers

int write_PNG(char* filepath, simple_PNG_p in); //write a struct simple_PNG to file
int write_chunk(FILE* fp, chunk_p in); //write a struct chunk to file
  

/* you're free to design and declare your own functions prototypes here*/
U32 read_u32_be(const U8 *buf)
{
    return ((U32)buf[0] << 24) |
           ((U32)buf[1] << 16) |
           ((U32)buf[2] << 8) |
           (U32)buf[3];
}

void write_u32_be(U8 *buf, U32 value)
{
    buf[0] = (U8)(value >> 24);
    buf[1] = (U8)(value >> 16);
    buf[2] = (U8)(value >> 8);
    buf[3] = (U8)value;
}
int is_png(U8 *buf, size_t n){
// 89 50 4E 47 0D 0A 1A 0A
    const U8 front[8] = {137, 80, 78, 71, 13, 10, 26, 10};
     if (buf == NULL || n < PNG_SIG_SIZE) {
        return 0;
    }
    for (int i = 0; i < 8; i++) {
        if (buf[i] != front[i]) {
            return 0;
        }
    }
    
    return 1;
}

int get_png_data_IHDR(struct data_IHDR *out, FILE *fp, long offset, int whence)
{   U8 data[DATA_IHDR_SIZE];

    if (out == NULL || fp == NULL || fseek(fp, offset, whence) != 0) {
        return 0;
    }

    if (fread(data, 1, DATA_IHDR_SIZE, fp) != DATA_IHDR_SIZE) {
        return 0;
    }

    out->width = ((U32)data[0] << 24) |
                 ((U32)data[1] << 16) |
                 ((U32)data[2] << 8) |
                 (U32)data[3];

    out->height = ((U32)data[4] << 24) |
                  ((U32)data[5] << 16) |
                  ((U32)data[6] << 8) |
                  (U32)data[7];
    out->bit_depth = data[8];
    out->color_type = data[9];
    out->compression = data[10];
    out->filter = data[11];
    out->interlace = data[12];
    
    return 1;
}

int get_png_height(struct data_IHDR *buf)
{   if (buf == NULL){
    return 0;
    }
    return buf->height;
}

int get_png_width(struct data_IHDR *buf)
{   if (buf == NULL){
    return 0;
    }
    return buf->width;
}


int get_png_chunks(simple_PNG_p out, FILE *fp, long offset, int whence)
{
    chunk_p current = NULL;

    if (out == NULL || fp == NULL || fseek(fp, offset, whence) != 0) {
        return 0;
    }

    out->p_IHDR = NULL;
    out->p_IDAT = NULL;
    out->p_IEND = NULL;

    while (1) {
        current = get_chunk(fp);
        if (current == NULL) {
            return 0;
        }

        if (memcmp(current->type, "IHDR", 4) == 0) {
            out->p_IHDR = current;
        } else if (memcmp(current->type, "IDAT", 4) == 0) {
            if (out->p_IDAT == NULL) {
                out->p_IDAT = current;
            } else {
                free_chunk(current);
            }
        } else if (memcmp(current->type, "IEND", 4) == 0) {
            out->p_IEND = current;
            break;
        } else {
            free_chunk(current);
        }
    }

    return (out->p_IHDR != NULL && out->p_IEND != NULL) ? 1 : 0;
}

chunk_p get_chunk(FILE *fp)
{
    chunk_p c = NULL;
    U8 len_raw[CHUNK_LEN_SIZE];
    U8 crc_raw[CHUNK_CRC_SIZE];
    U32 length;

    if (fp == NULL) {
        return NULL;
    }

    c = (chunk_p)malloc(sizeof(*c));
    if (c == NULL) {
        return NULL;
    }
    memset(c, 0, sizeof(*c));

    if (fread(len_raw, 1, CHUNK_LEN_SIZE, fp) != CHUNK_LEN_SIZE) {
        free_chunk(c);
        return NULL;
    }
    length = read_u32_be(len_raw);
    c->length = length;

    if (fread(c->type, 1, CHUNK_TYPE_SIZE, fp) != CHUNK_TYPE_SIZE) {
        free_chunk(c);
        return NULL;
    }

    if (length > 0) {
        c->p_data = (U8 *)malloc(length);
        if (c->p_data == NULL) {
            free_chunk(c);
            return NULL;
        }
        if (fread(c->p_data, 1, length, fp) != length) {
            free_chunk(c);
            return NULL;
        }
    }

    if (fread(crc_raw, 1, CHUNK_CRC_SIZE, fp) != CHUNK_CRC_SIZE) {
        free_chunk(c);
        return NULL;
    }
    c->crc = read_u32_be(crc_raw);

    return c;
}

U32 get_chunk_crc(chunk_p in)
{
    return (in == NULL) ? 0 : in->crc;
}

U32 calculate_chunk_crc(chunk_p in)
{
    U8 *buf = NULL;
    U32 crc_val = 0;
    size_t total_size = 0;

    if (in == NULL) {
        return 0;
    }

    total_size = CHUNK_TYPE_SIZE + in->length;
    buf = (U8 *)malloc(total_size);
    if (buf == NULL) {
        return 0;
    }

    memcpy(buf, in->type, CHUNK_TYPE_SIZE);
    if (in->length > 0 && in->p_data != NULL) {
        memcpy(buf + CHUNK_TYPE_SIZE, in->p_data, in->length);
    }

    crc_val = crc(buf, (int)total_size);
    free(buf);
    return crc_val;
}

simple_PNG_p mallocPNG(void)
{
    simple_PNG_p png = (simple_PNG_p)malloc(sizeof(*png));
    if (png == NULL) {
        return NULL;
    }
    png->p_IHDR = NULL;
    png->p_IDAT = NULL;
    png->p_IEND = NULL;
    return png;
}

void free_chunk(chunk_p in)
{
    if (in == NULL) {
        return;
    }
    free(in->p_data);
    free(in);
}

void free_png(simple_PNG_p in)
{
    if (in == NULL) {
        return;
    }
    free_chunk(in->p_IHDR);
    free_chunk(in->p_IDAT);
    free_chunk(in->p_IEND);
    free(in);
}

int write_chunk(FILE *fp, chunk_p in)
{
    U8 len_buf[CHUNK_LEN_SIZE];
    U8 crc_buf[CHUNK_CRC_SIZE];
    U32 crc_val;

    if (fp == NULL || in == NULL) {
        return 0;
    }

    write_u32_be(len_buf, in->length);
    if (fwrite(len_buf, 1, CHUNK_LEN_SIZE, fp) != CHUNK_LEN_SIZE) {
        return 0;
    }

    if (fwrite(in->type, 1, CHUNK_TYPE_SIZE, fp) != CHUNK_TYPE_SIZE) {
        return 0;
    }

    if (in->length > 0 && in->p_data != NULL) {
        if (fwrite(in->p_data, 1, in->length, fp) != in->length) {
            return 0;
        }
    }

    crc_val = calculate_chunk_crc(in);
    write_u32_be(crc_buf, crc_val);
    if (fwrite(crc_buf, 1, CHUNK_CRC_SIZE, fp) != CHUNK_CRC_SIZE) {
        return 0;
    }

    return 1;
}

int write_PNG(char *filepath, simple_PNG_p in)
{
    static const U8 png_sig[PNG_SIG_SIZE] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    FILE *fp = NULL;

    if (filepath == NULL || in == NULL) {
        return 0;
    }

    fp = fopen(filepath, "wb");
    if (fp == NULL) {
        return 0;
    }

    if (fwrite(png_sig, 1, PNG_SIG_SIZE, fp) != PNG_SIG_SIZE) {
        fclose(fp);
        return 0;
    }

    if (in->p_IHDR != NULL && write_chunk(fp, in->p_IHDR) == 0) {
        fclose(fp);
        return 0;
    }

    if (in->p_IDAT != NULL && write_chunk(fp, in->p_IDAT) == 0) {
        fclose(fp);
        return 0;
    }

    if (in->p_IEND != NULL && write_chunk(fp, in->p_IEND) == 0) {
        fclose(fp);
        return 0;
    }

    fclose(fp);
    return 1;
}
