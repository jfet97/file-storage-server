#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "simple_queue.h"
#include <stddef.h>
#include <pthread.h>

#define NON_ZERO_DO(code, todo) \
    if (code != 0)              \
    {                           \
        todo;                   \
    }

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// CREATE OR DELETE A NODE

SimpleNode *SimpleNode_create(void *element, SimpleNode *next, SimpleNode *prev)
{
    SimpleNode *newNode = malloc(sizeof(*newNode));

    if (newNode)
    {
        newNode->element = element;
        newNode->next = next;
        newNode->prev = prev;
    }

    return newNode;
}

void SimpleNode_free(SimpleNode **nodePtr)
{
    free(*nodePtr);
    *nodePtr = NULL;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// GET THE LENGTH

size_t List_length(List list)
{
    return list->length;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// CREATE - DELETE A LIST

List List_create()
{
    List newList = malloc(sizeof(*newList));

    if (newList != NULL)
    {
        newList->length = 0;
        newList->head = NULL;
        newList->tail = NULL;
    }

    return newList;
}

void List__free(SimpleNode **nodePtr)
{
    if (*nodePtr != NULL)
    {
        List__free(&(*nodePtr)->next);
        SimpleNode_free(nodePtr);
    }
}

void List_free(List *listPtr)
{
    List__free(&(*listPtr)->head);
    free(*listPtr);
    *listPtr = NULL;
}

SimpleNode *List__getNodeByIndex(SimpleNode *node, int index, int counter)
{
    if (node == NULL)
    {
        return NULL;
    }
    if (counter == index)
    {
        return node;
    }
    else
    {
        return List__getNodeByIndex(node->next, index, counter + 1);
    }
}

SimpleNode *List_getNodeByIndex(List list, int index)
{
    if (index < 0)
    {
        return NULL;
    }
    return List__getNodeByIndex(list->head, index, 0);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

// INSERT INTO THE TAIL

int List_insertTail(List list, void *element)
{ // O(1)
    SimpleNode *newNode = SimpleNode_create(element, NULL, list->tail);

    if (newNode == NULL)
    {
        return -1;
    }

    if (list->tail != NULL)
    { // list->tail == NULL <=> list.length == 0
        list->tail->next = newNode;
    }
    else
    {
        list->head = newNode;
    }

    list->tail = newNode;

    list->length++;

    return 0;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// DELETE HEAD

void List_deleteHead(List list)
{ // O(1)
    if (list->head == NULL)
    { // empty list
        return;
    }

    size_t listLength = List_length(list);
    if (listLength == 1)
    {
        list->tail = NULL;
    }

    SimpleNode *nodeToRemove = list->head;
    list->head = list->head->next;
    if (list->head != NULL)
    {
        list->head->prev = NULL;
    }
    SimpleNode_free(&nodeToRemove);

    list->length--;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// CREATE DELETE A QUEUE

SimpleQueue *SimpleQueue_create(int *error)
{
    int errToSet = 0;
    SimpleQueue *queueToRet = malloc(sizeof(*queueToRet));

    if (queueToRet == NULL)
    {
        errToSet = E_SQ_MALLOC;
    }

    if (!errToSet)
    {
        queueToRet->queue = List_create();

        if (queueToRet->queue == NULL)
        {
            errToSet = E_SQ_MALLOC;
        }
    }

    if (!errToSet)
    {
        NON_ZERO_DO(pthread_mutex_init(&queueToRet->lock, NULL), {
            errToSet = E_SQ_MUTEX_LOCK;
        })
    }

    if (!errToSet)
    {
        NON_ZERO_DO(pthread_cond_init(&queueToRet->read_cond, NULL), {
            errToSet = E_SQ_MUTEX_COND;
        })
    }

    if (errToSet)
    {
        (queueToRet && queueToRet->queue) ? free(queueToRet->queue) : NULL;
        queueToRet ? free(queueToRet) : NULL;
        queueToRet = NULL;
    }

    error && (*error = errToSet);
    return queueToRet;
}

void SimpleQueue_delete(SimpleQueue **queuePtr, int *error)
{

    int errToSet = 0;

    NON_ZERO_DO(pthread_mutex_lock(&((*queuePtr)->lock)), {
        errToSet = E_SQ_MUTEX_LOCK;
    })

    if (!errToSet)
    {
        List_free(&(*queuePtr)->queue);
        free(*queuePtr);
        *queuePtr = NULL;

        NON_ZERO_DO(pthread_mutex_unlock(&((*queuePtr)->lock)), {
            errToSet = E_SQ_MUTEX_LOCK;
        })
    }

    error && (*error = errToSet);
    return;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// ENQUEUE - DEQUEUE

void SimpleQueue_enqueue(SimpleQueue *queue, void *element, int *error)
{

    int errToSet = 0;

    NON_ZERO_DO(pthread_mutex_lock(&(queue->lock)), {
        errToSet = E_SQ_MUTEX_LOCK;
    })

    if (!errToSet)
    {
        NON_ZERO_DO(List_insertTail(queue->queue, element), {
            errToSet = E_SQ_GENERAL;
        })

        NON_ZERO_DO(pthread_cond_signal(&(queue->read_cond)), {
            errToSet = E_SQ_MUTEX_COND;
        })

        NON_ZERO_DO(pthread_mutex_unlock(&(queue->lock)), {
            errToSet = E_SQ_MUTEX_LOCK;
        })
    }

    error && (*error = errToSet);
    return;
}

void *SimpleQueue_dequeue(SimpleQueue *queue, int wait, int *error)
{

    int errToSet = 0;
    void *elementToRet = NULL;

    NON_ZERO_DO(pthread_mutex_lock(&(queue->lock)), {
        errToSet = E_SQ_MUTEX_LOCK;
    })

    if (!errToSet && queue->queue->length == 0 && !wait)
    {
        errToSet = E_SQ_EMPTY_QUEUE;
    }

    if (!errToSet && queue->queue->length == 0 && wait)
    {
        while (queue->queue->length == 0)
        {
            NON_ZERO_DO(pthread_cond_wait(&(queue->read_cond), &(queue->lock)), {
                errToSet = E_SQ_MUTEX_COND;
            })
        }
    }

    if (!errToSet)
    {
        SimpleNode *node = List_getNodeByIndex(queue->queue, 0);
        elementToRet = node->element;
        List_deleteHead(queue->queue);
    }

    if (!errToSet || errToSet != E_SQ_MUTEX_LOCK)
    {
        NON_ZERO_DO(pthread_mutex_unlock(&(queue->lock)), {
            errToSet = E_SQ_MUTEX_LOCK;
        })
    }

    error && (*error = errToSet);
    return elementToRet;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// QUEUE LENGTH

size_t SimpleQueue_length(SimpleQueue *queue, int *error)
{

    int errToSet = 0;
    size_t lenToRet = -1;

    NON_ZERO_DO(pthread_mutex_lock(&(queue->lock)), {
        errToSet = E_SQ_MUTEX_LOCK;
    })

    if (!errToSet)
    {
        lenToRet = List_length(queue->queue);

        NON_ZERO_DO(pthread_mutex_unlock(&(queue->lock)), {
            errToSet = E_SQ_MUTEX_LOCK;
        })
    }

    error && (*error = errToSet);
    return lenToRet;
}

const char *simple_queue_error_messages[] = {
    "",
    "queue internal malloc error",
    "queue internal mutex lock error",
    "queue internal general error",
    "queue is empty",
    "queue internal mutex cond error",
};

inline const char *SimpleQueue_getErrorMessage(int errorCode)
{
    return simple_queue_error_messages[errorCode];
}