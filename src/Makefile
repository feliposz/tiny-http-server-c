CC = gcc
CFLAGS = -Wall -g -std=c11

all: tiny

mynetlib.o: mynetlib.c mynetlib.h
	$(CC) $(CFLAGS) -c mynetlib.c

tiny: tiny.c mynetlib.o
	$(CC) $(CFLAGS) -o tiny tiny.c mynetlib.o

clean:
	rm -f tiny *.o