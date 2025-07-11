CC = gcc
CFLAGS = -Wall -g -std=c11

all: tiny hello adder

mynetlib.o: mynetlib.c mynetlib.h
	$(CC) $(CFLAGS) -c mynetlib.c

tiny: tiny.c mynetlib.o
	$(CC) $(CFLAGS) -o tiny tiny.c mynetlib.o

hello: cgi/hello.c
	mkdir -p ../wwwroot/cgi-bin
	$(CC) $(CFLAGS) -o ../wwwroot/cgi-bin/hello cgi/hello.c

adder: cgi/adder.c
	mkdir -p ../wwwroot/cgi-bin
	$(CC) $(CFLAGS) -o ../wwwroot/cgi-bin/adder cgi/adder.c

clean:
	rm -f tiny ../wwwroot/cgi-bin/* *.o