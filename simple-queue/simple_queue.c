#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "simple_queue.h"
#include <stddef.h>
#include <pthread.h>
#include <errno.h>

#define NON_ZERO_DO(code, todo) \
    if (code != 0)              \
    {                           \
        todo;                   \
    }

// -------------------------------
// -------------------------------
// DATA DEFINITIONS
typedef struct SimpleNode
{
    void *element;
    struct SimpleNode *next;
    struct SimpleNode *prev;
} SimpleNode;

typedef struct
{
    SimpleNode *head;
    SimpleNode *tail;
    size_t length;
} * SimpleList;

struct SimpleQueue
{
    SimpleList queue;
};

typedef struct
{
    pthread_mutex_t lock;
    pthread_cond_t read_cond;
    int to_be_canceled_soon;
} QueueData;

typedef struct InternalListNode
{
    SimpleQueue key;
    QueueData *value;
    struct InternalListNode *next;
} InternalListNode;

// -------------------------------
// -------------------------------
// INTERNALS

InternalListNode *internal_queues_data_list = NULL;

pthread_mutex_t iqdl_lock = PTHREAD_MUTEX_INITIALIZER;

static int insert(InternalListNode **listPtr, SimpleQueue key, QueueData *value)
{
    int codeToRet = 0;

    InternalListNode *new_node = malloc(sizeof(*new_node));
    if (new_node == NULL)
    {
        codeToRet = -1;
    }
    else
    {
        new_node->key = key;
        new_node->value = value;
        new_node->next = *listPtr;
        *listPtr = new_node;
    }

    return codeToRet;
}

static QueueData *searchByKey(InternalListNode *list, SimpleQueue key)
{
    if (list == NULL)
    {
        return NULL;
    }
    else if (list->key == key)
    {
        return list->value;
    }
    else
    {
        return searchByKey(list->next, key);
    }
}

static QueueData *deleteByKey(InternalListNode **listPtr, SimpleQueue key)
{
    if (listPtr == NULL)
    {
        return NULL;
    }
    else if ((*listPtr)->key == key)
    {
        InternalListNode *toFree = *listPtr;
        QueueData *toRet = toFree->value;
        *listPtr = (*listPtr)->next;
        free(toFree);
        return toRet;
    }
    else if ((*listPtr)->next != NULL)
    {
        return deleteByKey(&(*listPtr)->next, key);
    }
    else
    {
        return NULL;
    }
}

static SimpleNode *SimpleNode_create(void *element, SimpleNode *next, SimpleNode *prev)
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

static void SimpleNode_free(SimpleNode **nodePtr)
{
    free(*nodePtr);
    *nodePtr = NULL;
}

static size_t List_length(SimpleList list)
{
    return list->length;
}

static SimpleList List_create()
{
    SimpleList newList = malloc(sizeof(*newList));

    if (newList != NULL)
    {
        newList->length = 0;
        newList->head = NULL;
        newList->tail = NULL;
    }

    return newList;
}

static void List__free(SimpleNode **nodePtr)
{
    if (*nodePtr != NULL)
    {
        List__free(&(*nodePtr)->next);
        free((*nodePtr)->element);
        SimpleNode_free(nodePtr);
    }
}

static void List_free(SimpleList *listPtr)
{
    List__free(&(*listPtr)->head);
    free(*listPtr);
    *listPtr = NULL;
}

static SimpleNode *List__getNodeByIndex(SimpleNode *node, int index, int counter)
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

static SimpleNode *List_getNodeByIndex(SimpleList list, int index)
{
    if (index < 0)
    {
        return NULL;
    }
    return List__getNodeByIndex(list->head, index, 0);
}

static int List_insertTail(SimpleList list, void *element)
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

static void List_deleteHead(SimpleList list)
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

// -------------------------------
// -------------------------------
// API

SimpleQueue SimpleQueue_create(int *error)
{
    int errToSet = 0;
    SimpleQueue queueToRet = malloc(sizeof(*queueToRet));
    QueueData *queueData = NULL;

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
        queueData = malloc(sizeof(*queueData));
        if (queueData == NULL)
        {
            errToSet = E_SQ_MALLOC;
        }
    }

    if (!errToSet)
    {
        queueData->to_be_canceled_soon = 0;

        NON_ZERO_DO(insert(&internal_queues_data_list, queueToRet, queueData), {
            errToSet = E_SQ_GENERAL;
        })

        NON_ZERO_DO(pthread_mutex_init(&queueData->lock, NULL), {
            errToSet = E_SQ_MUTEX_LOCK;
        })

        NON_ZERO_DO(pthread_cond_init(&queueData->read_cond, NULL), {
            errToSet = E_SQ_MUTEX_COND;
        })
    }

    if (errToSet)
    {
        queueData ? deleteByKey(&internal_queues_data_list, queueToRet) : NULL;
        queueData ? free(queueData) : (void)NULL;
        (queueToRet && queueToRet->queue) ? free(queueToRet->queue) : (void)NULL;
        queueToRet ? free(queueToRet) : (void)NULL;

        queueToRet = NULL;
    }

    error && (*error = errToSet);
    return queueToRet;
}

void SimpleQueue_delete(SimpleQueue *queuePtr, int *error)
{

    int errToSet = 0;
    int hasQueueLock = 0;
    int hasDictLock = 0;
    SimpleQueue queue = NULL;
    QueueData *queue_data = NULL;

    if (queuePtr == NULL || *queuePtr == NULL)
    {
        errToSet = E_SQ_NO_QUEUE;
    }
    else
    {
        queue = *queuePtr;
    }

    if (!errToSet)
    {
        hasDictLock = 1;
        NON_ZERO_DO(pthread_mutex_lock(&iqdl_lock),
                    {
                        errToSet = E_SQ_MUTEX_LOCK;
                        hasDictLock = 0;
                    })
    }

    if (!errToSet)
    {
        // printf("queue: %p --- lista interna: %p\n", queue, internal_queues_data_list);
        queue_data = searchByKey(internal_queues_data_list, queue);
        if (queue_data == NULL)
        {
            errToSet = E_SQ_NO_QUEUE;
        }
    }

    if (!errToSet)
    {
        if (queue_data->to_be_canceled_soon)
        {
            errToSet = E_SQ_QUEUE_DELETED;
        }
    }

    if (!errToSet)
    {
        if (!queue_data->to_be_canceled_soon)
        {
            hasQueueLock = 1;
            NON_ZERO_DO(pthread_mutex_lock(&(queue_data->lock)),
                        {
                            errToSet = E_SQ_MUTEX_LOCK;
                            hasQueueLock = 0;
                        })
        }
    }

    if (hasQueueLock)
    {
        queue_data->to_be_canceled_soon = 1;

        // sveglio con broadcast tutti i lettori eventualmente fermi
        NON_ZERO_DO(pthread_cond_broadcast(&(queue_data)->read_cond), {
            errToSet = E_SQ_MUTEX_LOCK;
        })

        hasQueueLock = 0;
        // lascio la lock sulla queue
        NON_ZERO_DO(pthread_mutex_unlock(&(queue_data->lock)),
                    {
                        errToSet = E_SQ_MUTEX_LOCK;
                        hasQueueLock = 1;
                    })
    }

    int pmd_res = 0;
    while (!errToSet && ((pmd_res = pthread_mutex_destroy(&(queue_data)->lock)) == EBUSY))
    {
        hasQueueLock = 1;
        // lascio la possibilita' a thread fermi di accorgersi che stiamo chiudendo

        NON_ZERO_DO(pthread_mutex_lock(&(queue_data->lock)),
                    {
                        errToSet = E_SQ_MUTEX_LOCK;
                        hasQueueLock = 0;
                    })

        // sveglio con broadcast tutti i lettori eventualmente fermi
        NON_ZERO_DO(pthread_cond_broadcast(&(queue_data)->read_cond), {
            errToSet = E_SQ_MUTEX_LOCK;
        })

        if (hasQueueLock && !errToSet)
        {
            hasQueueLock = 0;
            NON_ZERO_DO(pthread_mutex_unlock(&(queue_data->lock)),
                        {
                            errToSet = E_SQ_MUTEX_LOCK;
                            hasQueueLock = 1;
                        })
        }
    }

    if (pmd_res != 0)
    {
        errToSet = E_SQ_MUTEX_LOCK;
    }

    if (errToSet != E_SQ_NO_QUEUE && errToSet != E_SQ_QUEUE_DELETED)
    {

        free(queue_data);
        deleteByKey(&internal_queues_data_list, queue);
        List_free(&(*queuePtr)->queue);
        free(*queuePtr);
        *queuePtr = NULL;
        // puts("!!! CANCELED !!!");
        fflush(stdout);
    }

    if (hasDictLock)
    {
        NON_ZERO_DO(pthread_mutex_unlock(&iqdl_lock), {
            errToSet = E_SQ_MUTEX_LOCK;
        })
    }

    error && (*error = errToSet);
    return;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// ENQUEUE - DEQUEUE

void SimpleQueue_enqueue(SimpleQueue queue, void *element, int *error)
{
    int errToSet = 0;
    int hasQueueLock = 0;
    int hasDictLock = 0;
    QueueData *queue_data = NULL;

    if (queue == NULL)
    {
        errToSet = E_SQ_NO_QUEUE;
    }

    if (queue != NULL && element == NULL)
    {
        errToSet = E_SQ_NULL_ELEMENT;
    }

    if (!errToSet)
    {
        hasDictLock = 1;
        NON_ZERO_DO(pthread_mutex_lock(&iqdl_lock),
                    {
                        errToSet = E_SQ_MUTEX_LOCK;
                        hasDictLock = 0;
                    })
    }

    if (!errToSet)
    {
        queue_data = searchByKey(internal_queues_data_list, queue);
        if (queue_data == NULL)
        {
            errToSet = E_SQ_NO_QUEUE;
        }
    }

    if (!errToSet)
    {
        if (queue_data->to_be_canceled_soon)
        {
            errToSet = E_SQ_QUEUE_DELETED;
        }
    }

    if (!errToSet)
    {
        hasQueueLock = 1;
        NON_ZERO_DO(pthread_mutex_lock(&(queue_data->lock)),
                    {
                        errToSet = E_SQ_MUTEX_LOCK;
                        hasQueueLock = 0;
                    })
    }

    if (hasDictLock)
    {
        NON_ZERO_DO(pthread_mutex_unlock(&iqdl_lock), {
            errToSet = E_SQ_MUTEX_LOCK;
        })
    }

    if (!errToSet)
    {
        NON_ZERO_DO(List_insertTail(queue->queue, element), {
            errToSet = E_SQ_GENERAL;
        })

        NON_ZERO_DO(pthread_cond_signal(&(queue_data->read_cond)), {
            errToSet = E_SQ_MUTEX_COND;
        })
    }

    if (hasQueueLock)
    {
        NON_ZERO_DO(pthread_mutex_unlock(&(queue_data->lock)), {
            errToSet = E_SQ_MUTEX_LOCK;
        })
    }

    error && (*error = errToSet);
    return;
}

void *SimpleQueue_dequeue(SimpleQueue queue, int wait, int *error)
{

    int errToSet = 0;
    void *elementToRet = NULL;
    int hasQueueLock = 0;
    int hasDictLock = 0;
    QueueData *queue_data = NULL;

    if (queue == NULL)
    {
        errToSet = E_SQ_NO_QUEUE;
    }

    if (!errToSet)
    {
        hasDictLock = 1;
        NON_ZERO_DO(pthread_mutex_lock(&iqdl_lock),
                    {
                        errToSet = E_SQ_MUTEX_LOCK;
                        hasDictLock = 0;
                    })
    }

    if (!errToSet)
    {
        queue_data = searchByKey(internal_queues_data_list, queue);
        if (queue_data == NULL)
        {
            errToSet = E_SQ_NO_QUEUE;
        }
    }

    if (!errToSet)
    {
        if (queue_data->to_be_canceled_soon)
        {
            errToSet = E_SQ_QUEUE_DELETED;
        }
    }

    if (!errToSet)
    {
        hasQueueLock = 1;
        NON_ZERO_DO(pthread_mutex_lock(&(queue_data->lock)),
                    {
                        errToSet = E_SQ_MUTEX_LOCK;
                        hasQueueLock = 0;
                    })
    }

    if (hasDictLock)
    {
        NON_ZERO_DO(pthread_mutex_unlock(&iqdl_lock), {
            errToSet = E_SQ_MUTEX_LOCK;
        })
    }

    if (!errToSet && queue->queue->length == 0 && !wait)
    {
        errToSet = E_SQ_EMPTY_QUEUE;
    }

    if (!errToSet && queue->queue->length == 0 && wait)
    {
        while (!errToSet && queue->queue->length == 0 && !queue_data->to_be_canceled_soon)
        {
            NON_ZERO_DO(pthread_cond_wait(&(queue_data->read_cond), &(queue_data->lock)), {
                errToSet = E_SQ_MUTEX_COND;
            })
        }
    }

    if (!errToSet && !queue_data->to_be_canceled_soon)
    {
        SimpleNode *node = List_getNodeByIndex(queue->queue, 0);
        elementToRet = node->element;
        List_deleteHead(queue->queue);
    }

    if (!errToSet && queue_data->to_be_canceled_soon)
    {
        errToSet = E_SQ_QUEUE_DELETED;
    }

    if (hasQueueLock)
    {
        NON_ZERO_DO(pthread_mutex_unlock(&(queue_data->lock)), {
            errToSet = E_SQ_MUTEX_LOCK;
        })
    }

    error && (*error = errToSet);
    return elementToRet;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// QUEUE LENGTH

size_t SimpleQueue_length(SimpleQueue queue, int *error)
{

    int errToSet = 0;
    size_t lenToRet = -1;
    int hasQueueLock = 0;
    int hasDictLock = 0;
    QueueData *queue_data = NULL;

    if (queue == NULL)
    {
        errToSet = E_SQ_NO_QUEUE;
    }

    if (!errToSet)
    {
        hasDictLock = 1;
        NON_ZERO_DO(pthread_mutex_lock(&iqdl_lock),
                    {
                        errToSet = E_SQ_MUTEX_LOCK;
                        hasDictLock = 0;
                    })
    }

    if (!errToSet)
    {
        queue_data = searchByKey(internal_queues_data_list, queue);
        if (queue_data == NULL)
        {
            errToSet = E_SQ_NO_QUEUE;
        }
    }

    if (!errToSet)
    {
        if (queue_data->to_be_canceled_soon)
        {
            errToSet = E_SQ_QUEUE_DELETED;
        }
    }

    if (!errToSet)
    {
        hasQueueLock = 1;
        NON_ZERO_DO(pthread_mutex_lock(&(queue_data->lock)),
                    {
                        errToSet = E_SQ_MUTEX_LOCK;
                        hasQueueLock = 0;
                    })
    }

    if (hasDictLock)
    {
        NON_ZERO_DO(pthread_mutex_unlock(&iqdl_lock), {
            errToSet = E_SQ_MUTEX_LOCK;
        })
    }

    if (!errToSet)
    {
        lenToRet = List_length(queue->queue);
    }

    if (hasQueueLock)
    {
        NON_ZERO_DO(pthread_mutex_unlock(&(queue_data->lock)), {
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
    "queue is going to be deleted soon",
    "queue is NULL",
    "queue cannot contain NULL elements",
};

inline const char *SimpleQueue_getErrorMessage(int errorCode)
{
    return simple_queue_error_messages[errorCode];
}