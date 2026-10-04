CC = gcc
CFLAGS = -Wall -Wextra -g -std=gnu11 -pthread
CPPFLAGS = -I../../lab1/png_util
LDLIBS = -lcurl -lz -lpthread
ifeq ($(OS),Windows_NT)
LDLIBS += -lws2_32
endif

TARGET = paster.exe
OBJS = paster.o paster_helpers.o crc.o zutil.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

paster.o: paster.c paster_helpers.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

paster_helpers.o: paster_helpers.c paster_helpers.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

crc.o: ../../lab1/png_util/crc.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

zutil.o: ../../lab1/png_util/zutil.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
