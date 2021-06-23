#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#include "config-parser.h"
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
        puts(ConfigParser_getErrorMessage(error));
        exit(EXIT_FAILURE);
    }
}

int main(void)
{

    int error;
    ConfigParser parser = ConfigParser_parse("./config.txt", &error);
    print(error);

    ConfigParser_printConfigs(parser, &error);
    print(error);

    ConfigParser_delete(&parser, &error);
    print(error);
    puts("well done!");
}