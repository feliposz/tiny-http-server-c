#include <stdlib.h>
#include "mysemqueue.h"

void queueInit(queue_t *pqueue, size_t capacity)
{
    pqueue->items = calloc(sizeof(int), capacity);
    pqueue->capacity = capacity;
    pqueue->front = 0;
    pqueue->rear = 0;
    sem_init(&pqueue->mutex, 0, 1);
    sem_init(&pqueue->emptySlots, 0, capacity);
    sem_init(&pqueue->availableItems, 0, 0);
}

void queueFree(queue_t *pqueue)
{
    free(pqueue->items);
}

void queueAdd(queue_t *pqueue, int item)
{
    sem_wait(&pqueue->emptySlots);
    sem_wait(&pqueue->mutex);
    pqueue->items[pqueue->rear] = item;
    pqueue->rear = (pqueue->rear + 1) % pqueue->capacity;
    sem_post(&pqueue->mutex);
    sem_post(&pqueue->availableItems);
}

int queueRemove(queue_t *pqueue)
{
    sem_wait(&pqueue->availableItems);
    sem_wait(&pqueue->mutex);
    int item = pqueue->items[pqueue->front];
    pqueue->front = (pqueue->front + 1) % pqueue->capacity;
    sem_post(&pqueue->mutex);
    sem_post(&pqueue->emptySlots);
    return item;
}