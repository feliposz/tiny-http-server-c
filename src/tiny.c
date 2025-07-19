// tiny.c - A very simple HTTP server
// This version implements concurrency by I/O multiplexing (select)

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
int verbose = 0; // TODO: add command line flag

long readRequestHeaders(buffered_reader_t *pbr, char *contentType, size_t contentTypeSize)
{
    long contentLength = 0;
    char headerLine[MAXLINE];
    for (int i = 0;; i++)
    {
        ssize_t headerLength = bufReadLine(pbr, headerLine, MAXLINE);
        if (headerLength == -1)
        {
            perror("bufReadLine");
            break;
        }
        else if (headerLength == 0)
        {
            printf("client closed connection\n");
            break;
        }
        if (strncmp(headerLine, "\r\n", headerLength) == 0)
        {
            break;
        }
        if (verbose)
        {
            printf("Header #%d: %.*s", i, (int)headerLength, headerLine);
        }
        if (strncasecmp(headerLine, "Content-Length:", 15) == 0)
        {
            char *split = strchr(headerLine, ':');
            contentLength = atol(split + 1);
        }
        if (strncasecmp(headerLine, "Content-Type:", 13) == 0)
        {
            char *split = strchr(headerLine, ':');
            split++;              // skip ':'
            while (*split == ' ') // skip whitespaces
                split++;
            size_t actualSize = headerLength - (split - headerLine) - 2; // -2 for CRLF
            snprintf(contentType, contentTypeSize, "%.*s", (int)actualSize, split);
        }
    }
    return contentLength;
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
    // TODO: check path traversal (../) with realpath
    // TODO: use snprintf where possible
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

    // TODO: handle large files by reading in blocks
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

    free(content);
}

void serveDynamic(int client, char *path, char *args, char *method, long inputLength, char *inputType, char *inputBuffer)
{
    printf("serving dynamic resource: %s with args: %s\n", path, args);
    if (checkResource(client, path, S_IXUSR, NULL) != 0)
    {
        return;
    }

    // set a pipe to allow for input redirection (stdin) to child process
    // parent will write the buffered input to it
    int pipefd[2] = {-1, -1};
    if (inputLength > 0 && inputBuffer != NULL)
    {
        if (verbose)
        {
            printf("input (%ld length): %.*s\n", inputLength, (int)inputLength, inputBuffer);
        }
        pipe(pipefd);
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
        if (pipefd[1] != -1)
        {
            close(pipefd[1]); // child doesn't need the write end of the pipe
        }
        if (pipefd[0] != -1)
        {
            if (verbose)
                printf("redirecting stdin to %d\n", pipefd[0]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
        }
        dup2(client, STDOUT_FILENO);
        setenv("QUERY_STRING", args, 1);
        setenv("REQUEST_METHOD", method, 1);
        char inputLengthString[20];
        sprintf(inputLengthString, "%ld", inputLength);
        setenv("CONTENT_LENGTH", inputLengthString, 1);
        setenv("CONTENT_TYPE", inputType, 1);
        execve(path, empty, environ);
        perror("execve"); // shouldn't return
        return;
    }
    else // parent
    {
        if (pipefd[0] != -1)
        {
            close(pipefd[0]); // parent doesn't need the read end of the pipe
        }
        if (pipefd[1] != -1)
        {
            if (verbose)
                printf("writing to %d\n", pipefd[1]);
            if (sendBytes(pipefd[1], inputBuffer, inputLength) == -1)
            {
                // TODO: kill child and return error to client
                perror("sendBytes");
            }
            close(pipefd[1]);
        }
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
    if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0 && strcmp(method, "POST") != 0)
    {
        errorResponse(client, 501, "Not Implemented", NULL);
        return;
    }
    char contentType[MAXLINE];
    strcpy(contentType, "");
    long contentLength = readRequestHeaders(&br, contentType, sizeof(contentType));
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
        char *inputBuffer = NULL;
        if (strcmp(method, "POST") == 0 && contentLength > 0)
        {
            if (verbose)
            {
                printf("contentLength=%ld\n", contentLength);
                printf("contentType=%s\n", contentType);
            }
            inputBuffer = malloc(contentLength);
            if (inputBuffer == NULL)
            {
                perror("malloc");
                errorResponse(client, 500, "Internal Server Error", NULL);
                return;
            }
            if (bufReadBytes(&br, inputBuffer, contentLength) == -1)
            {
                perror("bufReadBytes");
                errorResponse(client, 500, "Internal Server Error", NULL);
                return;
            }
        }
        serveDynamic(client, path, args, method, contentLength, contentType, inputBuffer);
        free(inputBuffer);
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

typedef struct
{
    fd_set readableSet;
    fd_set readyForReadingSet;
    int readyCount;
    int minFileDescriptor;
    int maxFileDescriptor;
    int fileDescriptors[FD_SETSIZE];
    buffered_reader_t readers[FD_SETSIZE];
} pool_t;

void initPool(pool_t *pool, int listenSocket)
{
    pool->minFileDescriptor = listenSocket;
    pool->maxFileDescriptor = listenSocket;
    FD_ZERO(&pool->readableSet);
    FD_SET(listenSocket, &pool->readableSet);
    for (int i = 0; i < FD_SETSIZE; i++)
    {
        pool->fileDescriptors[i] = -1;
    }
}

// add client to the pool
// returns 1 if successful, else 0
int addClient(pool_t *pool, int client)
{
    for (int i = 0; i < FD_SETSIZE; i++)
    {
        if (pool->fileDescriptors[i] == -1)
        {
            FD_SET(client, &pool->readableSet);
            pool->fileDescriptors[i] = client;
            bufReaderInit(&pool->readers[i], client);
            if (pool->maxFileDescriptor < client)
            {
                pool->maxFileDescriptor = client;
            }
            return 1;
        }
    }
    return 0;
}

void removeClient(pool_t *pool, int index)
{
    int client = pool->fileDescriptors[index];
    pool->fileDescriptors[index] = -1;
    FD_CLR(client, &pool->readableSet);
    close(client);
    if (client == pool->maxFileDescriptor)
    {
        int maxFD = -1;
        for (int i = 0; i < FD_SETSIZE; i++)
        {
            if (maxFD < pool->fileDescriptors[i])
            {
                maxFD = pool->fileDescriptors[i];
            }
        }
        pool->maxFileDescriptor = maxFD == -1 ? pool->minFileDescriptor : maxFD;
    }
}

void checkClients(pool_t *pool)
{
    for (int i = 0; i < FD_SETSIZE && pool->readyCount > 0; i++)
    {
        int client = pool->fileDescriptors[i];
        if (!FD_ISSET(client, &pool->readyForReadingSet))
        {
            continue;
        }
        printf("begin serving client %d\n", client);
        handleClient(client);
        removeClient(pool, i);
        pool->readyCount--;
        printf("end serving client %d\n", client);
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

    Signal(SIGCHLD, sigchldHandler);

    // avoid crashing the server if trying to write to client that prematurely closed the connection
    Signal(SIGPIPE, SIG_IGN);

    static pool_t pool;
    initPool(&pool, listenSocket);

    printf("listening on port %s\n", port);
    while (1)
    {
        pool.readyForReadingSet = pool.readableSet;
        printf("waiting on FD - max: %d\n", pool.maxFileDescriptor);
        pool.readyCount = select(pool.maxFileDescriptor + 1, &pool.readyForReadingSet, NULL, NULL, NULL);
        if (pool.readyCount == -1)
        {
            perror("select");
            continue;
        }
        printf("pool.readyCount = %d\n", pool.readyCount);

        if (FD_ISSET(listenSocket, &pool.readyForReadingSet))
        {
            struct sockaddr_storage address;
            socklen_t addressLength = sizeof(address);
            int client = accept(listenSocket, (SA *)&address, &addressLength);
            if (client == -1)
            {
                perror("accept");
            }
            else
            {
                int result;
                char clientHost[MAXHOST];
                char clientPort[MAXPORT];
                if ((result = getnameinfo((SA *)&address, addressLength, clientHost, MAXHOST, clientPort, MAXPORT, NI_NUMERICSERV)) != 0)
                {
                    fprintf(stderr, "error getting name information: %s\n", gai_strerror(result));
                }
                else
                {
                    printf("client %d connected %s:%s\n", client, clientHost, clientPort);
                }
                if (!addClient(&pool, client))
                {
                    close(client);
                }
            }
        }

        checkClients(&pool);
    }

    // unreachable
    close(listenSocket);
    return 0;
}