#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#include "file-system.h"
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

void print(int error)
{
    if (error)
    {
        puts(FileSystem_getErrorMessage(error));
        exit(EXIT_FAILURE);
    }
}

int main(void)
{

    int error;
    FileSystem fs = FileSystem_create(1024, 5, FS_REPLACEMENT_FIFO, &error);

    puts("well done!");
}