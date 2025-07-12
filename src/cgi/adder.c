#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLINE 1024

int main(void)
{
    char contentOutput[MAXLINE];
    char *postInput = NULL;
    int a = 0, b = 0;

    char *queryString = getenv("QUERY_STRING");
    char *method = getenv("REQUEST_METHOD");
#ifdef DEBUG
    fprintf(stderr, "method=%s\n", method);
    fprintf(stderr, "length=%s\n", getenv("CONTENT_LENGTH"));
    fprintf(stderr, "type=%s\n", getenv("CONTENT_TYPE"));
#endif
    char *input = NULL;
    if (method != NULL)
    {
        if (strcmp(method, "GET") == 0)
        {
            input = queryString;
        }
        else if (strcmp(method, "POST") == 0)
        {
            char *contentLengthString = getenv("CONTENT_LENGTH");
            long contentLength = atol(contentLengthString);
            if (contentLength > 0)
            {
                postInput = malloc(contentLength);
                if (postInput != NULL && fread(postInput, contentLength, 1, stdin) == 1)
                {
                    input = postInput;
                }
            }
        }
    }
    if (input != NULL)
    {
        // TODO: properly split arguments (strtok_r) and key/values accordingly
        // super lazy way to do this... it's just for testing! :P
        sscanf(input, "a=%d&b=%d", &a, &b);
    }

    sprintf(contentOutput, "%d + %d = %d", a, b, a + b);

    printf("Connection: close\r\n");
    printf("Content-length: %d\r\n", (int)strlen(contentOutput));
    printf("Content-type: text/plain\r\n\r\n");
    printf("%s", contentOutput);
    fflush(stdout);
    free(postInput);
    return 0;
}
