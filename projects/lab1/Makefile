CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99
CPPFLAGS = -I./png_util
LDLIBS = -lz

FINDPNG_TARGET = findpng.exe
CATPNG_TARGET = catpng.exe
FINDPNG_OBJS = main_exec.o png_util/crc.o png_util/zutil.o
CATPNG_OBJS = catpng.o png_util/png_concat.o png_util/crc.o png_util/zutil.o

.PHONY: all clean

all: $(FINDPNG_TARGET) $(CATPNG_TARGET)

$(FINDPNG_TARGET): $(FINDPNG_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(CATPNG_TARGET): $(CATPNG_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

main_exec.o: main.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(FINDPNG_TARGET) $(CATPNG_TARGET) $(FINDPNG_OBJS) $(CATPNG_OBJS)