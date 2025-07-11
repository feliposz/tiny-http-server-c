#ifndef _MYNETLIB_H
#define _MYNETLIB_H

// Small utility library for networking

#include <sys/types.h>

#define BUFSIZ 8192 // size of internal buffer

typedef struct sockaddr SA;

// Structure for keeping track of the internal buffer for the reader
typedef struct
{
    int socket;
    size_t availableBytes;
    char *currOffset;
    char buffer[BUFSIZ];
} buffered_reader_t;

int clientConnect(char *host, char *port);
int serverListen(char *port);
ssize_t sendBytes(int socket, void *buf, size_t count);
ssize_t receiveBytes(int socket, void *buf, size_t count);
void bufReaderInit(buffered_reader_t *br, int socket);
int bufFill(buffered_reader_t *br);
ssize_t bufReadLine(buffered_reader_t *br, void *buf, size_t count);

#endif