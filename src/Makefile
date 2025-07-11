CC = gcc
CFLAGS = -Wall -g -std=c11 -D_POSIX_C_SOURCE=200112L

all: tiny

tiny: tiny.c
	$(CC) $(CFLAGS) -o tiny tiny.c

clean:
	rm -f tiny *.o