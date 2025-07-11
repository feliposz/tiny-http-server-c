#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "mynetlib.h"

#define BACKLOG 100

// Tries to connect to server at host and port.
// Returns a connected socket or -1 on error.
int clientConnect(char *host, char *port)
{
    int gaiResult;
    struct addrinfo hint, *ptrInfoList;
    memset(&hint, 0, sizeof(hint));
    hint.ai_socktype = SOCK_STREAM; // tcp
    if ((gaiResult = getaddrinfo(host, port, &hint, &ptrInfoList)) != 0)
    {
        fprintf(stderr, "error getting address information: %s\n", gai_strerror(gaiResult));
        return -1;
    }
    int clientSocket = -1;
    for (struct addrinfo *p = ptrInfoList; p != NULL; p = p->ai_next)
    {
        clientSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (clientSocket == -1)
            continue;
        if (connect(clientSocket, p->ai_addr, p->ai_addrlen) == 0)
            break;
        close(clientSocket);
        clientSocket = -1;
    }
    freeaddrinfo(ptrInfoList);
    return clientSocket;
}

// Tries to bind to a port and start listening for connections.
// Returns a connected socket or -1 on error.
int serverListen(char *port)
{
    int gaiResult;
    struct addrinfo hint, *ptrInfoList;
    int optval = 1;
    memset(&hint, 0, sizeof(hint));
    hint.ai_socktype = SOCK_STREAM; // tcp
    hint.ai_flags = AI_PASSIVE;
    if ((gaiResult = getaddrinfo(NULL, port, &hint, &ptrInfoList)) != 0)
    {
        fprintf(stderr, "error getting address information: %s\n", gai_strerror(gaiResult));
        return -1;
    }
    int listenSocket = -1;
    for (struct addrinfo *p = ptrInfoList; p != NULL; p = p->ai_next)
    {
        listenSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listenSocket == -1)
            continue;
        // avoid address already in use error for better debugging
        setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
        if (bind(listenSocket, p->ai_addr, p->ai_addrlen) == 0)
            break;
        close(listenSocket);
        listenSocket = -1;
    }
    freeaddrinfo(ptrInfoList);
    if (listen(listenSocket, BACKLOG) == -1)
    {
        close(listenSocket);
        return -1;
    }
    return listenSocket;
}

// Sends the entire contents of 'buf' with 'count' length to socket.
// Returns 'count' or -1 on error
ssize_t sendBytes(int socket, void *buf, size_t count)
{
    size_t remaining = count;
    char *offset = buf;
    while (remaining > 0)
    {
        ssize_t sent = write(socket, offset, remaining);
        if (sent == -1)
        {
            if (errno == EINTR)
            {
                sent = 0; // interrupted, try again
            }
            else
            {
                perror("write");
                return -1;
            }
        }
        offset += sent;
        remaining -= sent;
    }
    return count;
}

// Reads up to 'count' bytes from socket and writes it into buf.
// Returns total bytes read or -1 on error.
ssize_t receiveBytes(int socket, void *buf, size_t count)
{
    size_t remaining = count;
    char *offset = buf;
    while (remaining > 0)
    {
        ssize_t bytesRead = read(socket, offset, remaining);
        if (bytesRead == 0) // EOF
            break;
        if (bytesRead == -1)
        {
            if (errno == EINTR)
            {
                bytesRead = 0; // interrupted, try again
            }
            else
            {
                return -1;
            }
        }
        offset += bytesRead;
        remaining -= bytesRead;
    }
    return count - remaining;
}

// Initializes the control structure for buffered reader
void bufReaderInit(buffered_reader_t *br, int socket)
{
    br->socket = socket;
    br->availableBytes = 0;
    br->currOffset = br->buffer;
}

// Read from socket filling the internal buffer
int bufFill(buffered_reader_t *br)
{
    while (1)
    {
        ssize_t bytesRead = read(br->socket, br->buffer, sizeof(br->buffer));
        if (bytesRead == -1)
        {
            if (errno == EINTR) // interrupted, retry
                continue;
            else
                return -1;
        }
        else if (bytesRead == 0) // EOF
        {
            return 0;
        }
        br->availableBytes = bytesRead;
        br->currOffset = br->buffer;
        return bytesRead;
    }
}

// Reads a line up to 'count' (or until encountering '\n') from internal buffer and writes it into user buffer.
// Returns total bytes read or -1 on error.
ssize_t bufReadLine(buffered_reader_t *br, void *buf, size_t count)
{
    size_t remaining = count;
    char *writeOffset = buf;
    while (remaining > 0)
    {
        if (br->availableBytes == 0)
        {
            size_t result;
            if ((result = bufFill(br)) <= 0)
                return result;
        }
        char c = *br->currOffset++;
        *writeOffset++ = c;
        br->availableBytes--;
        remaining--;
        if (c == '\n')
            break;
    }
    return count - remaining;
}
