#include <stddef.h>
#include <pthread.h>

#ifndef SIMPLE_QUEUE__
#define SIMPLE_QUEUE__

#define E_SQ_MALLOC 1
#define E_SQ_MUTEX_LOCK 2
#define E_SQ_GENERAL 3
#define E_SQ_EMPTY_QUEUE 4
#define E_SQ_MUTEX_COND 5
#define E_SQ_QUEUE_DELETED 6
#define E_SQ_NO_QUEUE 7
#define E_SQ_NULL_ELEMENT 8

typedef struct SimpleQueue *SimpleQueue;

// queue
SimpleQueue SimpleQueue_create(int *error);
void SimpleQueue_delete(SimpleQueue *queuePtr, int *error);
void SimpleQueue_enqueue(SimpleQueue queue, void *element, int *error);
void *SimpleQueue_dequeue(SimpleQueue queue, int wait, int *error);
size_t SimpleQueue_length(SimpleQueue queue, int *error);
const char *SimpleQueue_getErrorMessage(int errorCode);
#endif