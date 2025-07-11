// tiny.c - A very simple HTTP server

#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "mynetlib.h"

#define MAXLINE 1024
#define MAXHOST 100
#define MAXPORT 100

void handleClient(int client)
{
    char line[MAXLINE];
    buffered_reader_t br;
    bufReaderInit(&br, client);
    while (1)
    {
        ssize_t length = bufReadLine(&br, line, MAXLINE);
        if (length == -1)
        {
            perror("receiveLine");
        }
        else if (length == 0)
        {
            printf("client closed connection\n");
            break;
        }
        printf("received %ld bytes: %.*s", length, (int)length, line);
        if (strncmp(line, "\r\n", length) == 0)
        {
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *port = argv[1];

    int listenSocket = serverListen(port);
    if (listenSocket == -1)
    {
        fprintf(stderr, "cant't listen on port %s\n", port);
        exit(EXIT_FAILURE);
    }

    char clientHost[MAXHOST];
    char clientPort[MAXPORT];

    printf("listening on port %s\n", port);
    while (1)
    {
        struct sockaddr_storage address;
        socklen_t addressLength = sizeof(address);
        int client = accept(listenSocket, (SA *)&address, &addressLength);
        if (client == -1)
        {
            perror("accept");
            continue;
        }
        int result;
        if ((result = getnameinfo((SA *)&address, addressLength, clientHost, MAXHOST, clientPort, MAXPORT, NI_NUMERICSERV)) != 0)
        {
            fprintf(stderr, "error getting name information: %s\n", gai_strerror(result));
        }
        else
        {
            printf("client connected %s:%s\n", clientHost, clientPort);
        }
        handleClient(client);
        close(client);
    }

    // unreachable
    close(listenSocket);
    return 0;
}