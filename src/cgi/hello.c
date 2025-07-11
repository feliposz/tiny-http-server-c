#include <stdio.h>
#include <string.h>

#define MAXLINE 1024

int main(void)
{
    char content[MAXLINE];

    sprintf(content, "Hello CGI!");

    printf("Connection: close\r\n");
    printf("Content-length: %d\r\n", (int)strlen(content));
    printf("Content-type: text/plain\r\n\r\n");
    printf("%s", content);
    fflush(stdout);

    return 0;
}
