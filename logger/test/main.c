#define _POSIX_C_SOURCE 200809L
#include "logger.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/syscall.h>
#include <sys/types.h>

#ifndef SYS_gettid
#error "SYS_gettid unavailable on this system"
#endif

#define gettid() ((pid_t)syscall(SYS_gettid))

void print(int error)
{
    if (error)
    {
        puts(Logger_getErrorMessage(error));
        exit(EXIT_FAILURE);
    }
}

void *thread(void *args)
{

    pid_t tid = gettid();

    int error = 0;

    for (int i = 0; i < 100; i++)
    {
        char buf[1000];
        sprintf(buf, "il thread con pid: %d dice ciao per la %d-esima volta ---", tid, i);
        Logger_log(buf, strlen(buf), &error);
        print(error);
    }

    return NULL;
}

int main(void)
{

    srand(time(NULL));

    unsigned int N = 300;

    int error = 0;
    Logger_create("./log.txt", &error);
    print(error);

    for (int i = 0; i < 100; i++)
    {
        char buf[100];
        sprintf(buf, "%s %d ---", "ciao", i);
        Logger_log(buf, strlen(buf), &error);
        print(error);
    }

    printf("running concurrency tests (this may take a while)...\n");

    pthread_t conc_tests[N];
    for (int i = 0; i < N; ++i)
    {
        pthread_create(&conc_tests[i], NULL, thread, NULL);
    }

    for (int i = 0; i < N; ++i)
    {
        pthread_join(conc_tests[i], NULL);
    }

    Logger_delete(1, &error);

    puts("well done!");
    print(error);
}