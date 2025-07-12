// tiny.c - A very simple HTTP server

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <linux/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include "mynetlib.h"

#define MAXLINE 1024
#define MAXHOST 100
#define MAXPORT 100

#define BASEDIR "./wwwroot"
#define DEFAULT_FILENAME "index.html"

extern char **environ;
int verbose = 0;

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
        if (verbose)
        {
            printf("Header #%d: %.*s", i, (int)length, headerLine);
        }
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
        snprintf(buf, MAXLINE, "Content-Length: %zu\r\n\r\n", strlen(longMessage));
        sendBytes(client, buf, strlen(buf));
        sendBytes(client, longMessage, strlen(longMessage));
    }
    else
    {
        char defaultMessage[MAXLINE];
        snprintf(defaultMessage, MAXLINE, "%d %s", statusCode, shortMessage);
        snprintf(buf, MAXLINE, "Content-Length: %zu\r\n\r\n", strlen(defaultMessage));
        sendBytes(client, buf, strlen(buf));
        sendBytes(client, defaultMessage, strlen(defaultMessage));
    }
}

int parseURI(char *uri, char *path, char *args)
{
    char *splitPos = strchr(uri, '?');
    strcpy(path, BASEDIR);
    if (splitPos == NULL)
    {
        strcat(path, uri);
        strcpy(args, "");
    }
    else
    {
        size_t pathLength = splitPos - uri;
        strncat(path, uri, pathLength);
        strcpy(args, splitPos + 1);
    }
    size_t len = strlen(path);
    if (path[len - 1] == '/')
    {
        strcat(path, DEFAULT_FILENAME);
    }
    int isDynamic = strstr(path, "/cgi-bin/") != NULL;
    return isDynamic;
}

char *getMimeTypeString(char *path)
{
    char *extension = strrchr(path, '.');
    if (extension == NULL)
    {
        return "text/plain";
    }
    if (strcmp(extension, ".html") == 0 || strcmp(extension, ".htm") == 0)
    {
        return "text/html";
    }
    if (strcmp(extension, ".jpeg") == 0 || strcmp(extension, ".jpg") == 0)
    {
        return "image/jpeg";
    }
    if (strcmp(extension, ".gif") == 0)
    {
        return "image/gif";
    }
    if (strcmp(extension, ".png") == 0)
    {
        return "image/png";
    }
    if (strcmp(extension, ".mp4") == 0)
    {
        return "video/mp4";
    }
    return "text/plain";
}

int checkResource(int client, char *path, mode_t flag, long *psize)
{
    struct stat fileinfo;
    if (stat(path, &fileinfo) == -1)
    {
        if (errno == ENOENT)
        {
            errorResponse(client, 404, "Not Found", NULL);
            return -1;
        }
        perror("stat");
        errorResponse(client, 500, "Internal Server Error", NULL);
        return -1;
    }
    if (!S_ISREG(fileinfo.st_mode) || (fileinfo.st_mode & flag) == 0)
    {
        errorResponse(client, 403, "Forbidden", NULL);
        return -1;
    }
    if (psize != NULL)
    {
        *psize = fileinfo.st_size;
    }
    return 0;
}

void serveHeadRequest(int client, char *path, int isDynamic)
{
    printf("serving head request: %s\n", path);
    long size;
    if (checkResource(client, path, S_IRUSR, &size) != 0)
    {
        return;
    }
    char buf[MAXLINE];
    snprintf(buf, MAXLINE, "HTTP/1.1 200 OK\r\n");
    sendBytes(client, buf, strlen(buf));
    if (isDynamic)
    {
        // unknown size: payload headers may be omitted according to RFC 7231
        sendBytes(client, "\r\n", 2);
        return;
    }
    snprintf(buf, MAXLINE, "Content-Type: %s\r\n", getMimeTypeString(path));
    sendBytes(client, buf, strlen(buf));
    snprintf(buf, MAXLINE, "Content-Length: %zu\r\n\r\n", size);
    sendBytes(client, buf, strlen(buf));
}

void serveStatic(int client, char *path)
{
    printf("serving static resource: %s\n", path);
    long size;
    if (checkResource(client, path, S_IRUSR, &size) != 0)
    {
        return;
    }

    FILE *file = fopen(path, "rb");
    char *content = malloc(size);
    fread(content, size, 1, file);
    fclose(file);

    char buf[MAXLINE];
    snprintf(buf, MAXLINE, "HTTP/1.1 200 OK\r\n");
    sendBytes(client, buf, strlen(buf));
    snprintf(buf, MAXLINE, "Content-Type: %s\r\n", getMimeTypeString(path));
    sendBytes(client, buf, strlen(buf));
    snprintf(buf, MAXLINE, "Content-Length: %zu\r\n\r\n", size);
    sendBytes(client, buf, strlen(buf));
    sendBytes(client, content, size);
    sendBytes(client, "\r\n", 2);

    free(content);
}

void serveDynamic(int client, char *path, char *args)
{
    printf("serving dynamic resource: %s with args: %s\n", path, args);
    if (checkResource(client, path, S_IXUSR, NULL) != 0)
    {
        return;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        errorResponse(client, 500, "Internal Server Error", NULL);
        return;
    }

    char buf[MAXLINE];
    snprintf(buf, MAXLINE, "HTTP/1.1 200 OK\r\n");
    sendBytes(client, buf, strlen(buf));

    if (pid == 0) // child
    {
        char *empty[] = {NULL};
        dup2(client, STDOUT_FILENO);
        setenv("QUERY_STRING", args, 1);
        execve(path, empty, environ);
        perror("fork"); // shouldn't return
        return;
    }
    else // parent
    {
        close(client); // avoid leaking resources
        printf("spawned child process pid=%d\n", pid);
    }
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
    if (verbose)
    {
        printf("Request - Method: %s URL: %s Version: %s\n", method, uri, version);
    }
    if (strcmp(version, "HTTP/1.1") != 0)
    {
        errorResponse(client, 505, "HTTP Version Not Supported", NULL);
        return;
    }
    if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0)
    {
        errorResponse(client, 501, "Not Implemented", NULL);
        return;
    }
    readRequestHeaders(&br);
    char path[MAXLINE], args[MAXLINE];
    int isDynamic = parseURI(uri, path, args);
    if (verbose)
    {
        printf("isDynamic: %d path: %s args: %s\n", isDynamic, path, args);
    }
    if (strcmp(method, "HEAD") == 0)
    {
        serveHeadRequest(client, path, isDynamic);
    }
    else if (!isDynamic)
    {
        serveStatic(client, path);
    }
    else
    {
        serveDynamic(client, path, args);
    }
}

void sigchldHandler(int sig)
{
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
    {
        if (WIFEXITED(status))
        {
            printf("child pid=%d exit status=%d\n", pid, WEXITSTATUS(status));
            return;
        }
        if (WIFSIGNALED(status))
        {
            printf("child pid=%d terminated by signal=%d\n", pid, WTERMSIG(status));
            return;
        }
    }
}

typedef void handler_t(int);

handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
        perror("sigaction");
    return (old_action.sa_handler);
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

    Signal(SIGCHLD, sigchldHandler);

    // avoid crashing the server if trying to write to client that prematurely closed the connection
    Signal(SIGPIPE, SIG_IGN);

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