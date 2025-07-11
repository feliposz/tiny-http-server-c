#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLINE 1024

int main(void)
{
    char content[MAXLINE];
    int a = 0, b = 0;

    char *queryString = getenv("QUERY_STRING");
    if (queryString != NULL)
    {
        // super lazy way to do this... it's just for testing! :P
        char *split = strchr(queryString, '&');
        a = atoi(queryString);
        b = atoi(split + 1);
    }

    sprintf(content, "%d + %d = %d", a, b, a + b);

    printf("Connection: close\r\n");
    printf("Content-length: %d\r\n", (int)strlen(content));
    printf("Content-type: text/plain\r\n\r\n");
    printf("%s", content);
    fflush(stdout);

    return 0;
}
