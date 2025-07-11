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

void readRequestHeaders(buffered_reader_t *pbr)
{
    char headerLine[MAXLINE];
    for (int i = 0;; i++)
    {
        ssize_t length = bufReadLine(pbr, headerLine, MAXLINE);
        if (length == -1)
        {
            perror("bufReadLine");
            break;
        }
        else if (length == 0)
        {
            printf("client closed connection\n");
            break;
        }
        if (strncmp(headerLine, "\r\n", length) == 0)
        {
            break;
        }
        printf("Header #%d: %.*s", i, (int)length, headerLine);
    }
}

void errorResponse(int client, int statusCode, char *shortMessage, char *longMessage)
{
    char buf[MAXLINE];
    snprintf(buf, MAXLINE, "HTTP/1.1 %d %s\r\n", statusCode, shortMessage);
    sendBytes(client, buf, strlen(buf));
    snprintf(buf, MAXLINE, "Content-Type: text/plain\r\n");
    sendBytes(client, buf, strlen(buf));
    if (longMessage != NULL)
    {
        snprintf(buf, MAXLINE, "Content-Length: %zu\r\n", strlen(longMessage));
        sendBytes(client, buf, strlen(buf));
        sendBytes(client, longMessage, strlen(longMessage));
    }
    else
    {
        char defaultMessage[MAXLINE];
        snprintf(defaultMessage, MAXLINE, "%d %s", statusCode, shortMessage);
        snprintf(buf, MAXLINE, "Content-Length: %zu\r\n", strlen(defaultMessage));
        sendBytes(client, buf, strlen(buf));
        sendBytes(client, defaultMessage, strlen(defaultMessage));
    }
}

int parseURI(char *uri, char *path, char *args)
{
    char *argsPos = strchr(uri, '?');
    if (argsPos == NULL)
    {
        strcpy(path, uri);
        strcpy(args, "");
    }
    else
    {
        size_t pathLength = argsPos - uri;
        strncpy(path, uri, pathLength);
        path[pathLength] = '\0';
        strcpy(args, argsPos + 1);
    }
    int isDynamic = strstr(path, "/cgi-bin/") != NULL;
    return isDynamic;
}

void handleClient(int client)
{
    char reqLine[MAXLINE];
    buffered_reader_t br;
    bufReaderInit(&br, client);
    ssize_t reqLineLength = bufReadLine(&br, reqLine, MAXLINE);
    if (reqLineLength == -1)
    {
        perror("bufReadLine");
        return;
    }
    else if (reqLineLength == 0)
    {
        printf("client closed connection\n");
        return;
    }
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    sscanf(reqLine, "%s %s %s", method, uri, version);
    printf("Request - Method: %s URL: %s Version: %s\n", method, uri, version);
    if (strcmp(version, "HTTP/1.1") != 0)
    {
        errorResponse(client, 505, "HTTP Version Not Supported", NULL);
        return;
    }
    if (strcmp(method, "GET") != 0)
    {
        errorResponse(client, 501, "Not Implemented", NULL);
        return;
    }
    readRequestHeaders(&br);
    char path[MAXLINE], args[MAXLINE];
    int isDynamic = parseURI(uri, path, args);
    printf("isDynamic: %d path: %s args: %s\n", isDynamic, path, args);
    // serve static content
    // serve dynamic (CGI) content
    errorResponse(client, 501, "Not Implemented", "server under development");
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