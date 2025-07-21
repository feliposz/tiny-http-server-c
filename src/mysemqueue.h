#ifndef _MYSEMQUEUE_H
#define _MYSEMQUEUE_H

// Simple thread-safe queue based on semaphores

#include <semaphore.h>

typedef struct
{
    int *items;
    int capacity;
    int front;
    int rear;
    sem_t mutex;
    sem_t emptySlots;
    sem_t availableItems;
} queue_t;

void queueInit(queue_t *, size_t);
void queueFree(queue_t *);
void queueAdd(queue_t *, int);
int queueRemove(queue_t *);

#endif