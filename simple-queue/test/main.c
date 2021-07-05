#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#include "simple_queue.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

typedef struct
{
    unsigned int n_of_threads;
    unsigned int num;
    unsigned int interrupt;
    void *unique_message;
    SimpleQueue queue;
} ConcTestData;

typedef struct
{
    unsigned int num_items;
    void *unique_message;
    SimpleQueue queue;
} ConcTestThreadData;

void *producer(void *arg)
{
    ConcTestThreadData *d = arg;
    unsigned int rand_state = time(NULL);
    int t = rand_r(&rand_state);
    for (int i = 0; i < d->num_items; ++i)
    {
        SimpleQueue_enqueue(d->queue, d->unique_message, NULL);
        usleep(((double)t / RAND_MAX) * 1000);
    }

    return NULL;
}

void *consumer(void *arg)
{
    ConcTestThreadData *d = arg;

    unsigned int rand_state = time(NULL);
    int t = rand_r(&rand_state);
    for (int i = 0; i < d->num_items; ++i)
    {
        int errore = 0;
        void *res = SimpleQueue_dequeue(d->queue, 1, &errore);
        if (errore)
        {
            // puts(SimpleQueue_getErrorMessage(errore));
        }
        else
        {
            assert(res == d->unique_message);
        }

        usleep(((double)t / RAND_MAX) * 1000);
    }

    return NULL;
}

void *concurrent_test(void *args)
{
    ConcTestData *tdp = args;

    unsigned int num_producers = tdp->n_of_threads;
    unsigned int num_consumers = tdp->n_of_threads;
    unsigned int num = tdp->num;

    pthread_t producers_tids[num_producers];
    pthread_t consumers_tids[num_consumers];

    ConcTestThreadData ttdsp[num_producers];
    ConcTestThreadData ttdsc[num_consumers];

    for (int i = 0; i < num_producers; ++i)
    {
        ttdsp[i].num_items = tdp->num;
        ttdsp[i].unique_message = tdp->unique_message;
        ttdsp[i].queue = tdp->queue;
        pthread_create(&producers_tids[i], NULL, producer, ttdsp + i);
    }
    for (int i = 0; i < num_consumers; ++i)
    {
        ttdsc[i].num_items = tdp->num;
        ttdsc[i].unique_message = tdp->unique_message;
        ttdsc[i].queue = tdp->queue;
        pthread_create(&consumers_tids[i], NULL, consumer, ttdsc + i);
    }

    if (tdp->interrupt)
    {
        // printf("%d\n", tdp->interrupt);
        sleep(num / 10);
        SimpleQueue_delete(&(tdp->queue), NULL);
    }

    for (int i = 0; i < num_producers; ++i)
    {
        pthread_join(producers_tids[i], NULL);
    }
    for (int i = 0; i < num_consumers; ++i)
    {
        pthread_join(consumers_tids[i], NULL);
    }

    return NULL;
}

int main(void)
{
    srand(time(NULL));

    unsigned int N = 1 + (rand() % 15);

    int queueError;
    SimpleQueue queue = SimpleQueue_create(&queueError);
    puts(SimpleQueue_getErrorMessage(queueError));

    int a, b, c, d;

    SimpleQueue_enqueue(queue, &a, NULL);
    SimpleQueue_enqueue(queue, &b, NULL);
    SimpleQueue_enqueue(queue, &c, NULL);
    SimpleQueue_enqueue(queue, &d, NULL);

    assert(SimpleQueue_dequeue(queue, 0, NULL) == &a);
    assert(SimpleQueue_dequeue(queue, 0, NULL) == &b);
    assert(SimpleQueue_dequeue(queue, 0, NULL) == &c);
    assert(SimpleQueue_dequeue(queue, 0, NULL) == &d);

    SimpleQueue_delete(&queue, NULL);

    printf("running concurrency tests (this may take a while)...\n");

    pthread_t conc_tests[N];
    ConcTestData tds[N];
    SimpleQueue queues[N];

    for (int i = 0; i < N; ++i)
    {
        queues[i] = SimpleQueue_create(&queueError);
    }

    for (int i = 0; i < N; ++i)
    {
        tds[i].n_of_threads = (i*i + N)/7;
        tds[i].num = i * 100;
        tds[i].interrupt = i % 2;
        tds[i].queue = queues[i];
        tds[i].unique_message = malloc(sizeof(char));

        pthread_create(&conc_tests[i], NULL, concurrent_test, tds + i);
    }

    for (int i = 0; i < N; ++i)
    {
        pthread_join(conc_tests[i], NULL);
    }
    for (int i = 0; i < N; ++i)
    {
        free(tds[i].unique_message);
        if (!tds[i].interrupt)
        {
            SimpleQueue_delete(queues + i, NULL);
        }
    }

    puts("well done!");
}