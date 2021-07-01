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

#define USED_POLICY FS_REPLACEMENT_FIFO
#define MAX_STORAGE_SIZE 1024
#define MAX_NUM_OF_FILES 3
#define PATH_FILE_1 "/folder1/file1.txt"
#define CONTENT_FILE_1 "ciao dal file 1 - test"
#define PATH_FILE_2 "/folder2/file2.txt"
#define CONTENT_FILE_2 "ciao dal file 2 - test"
#define PATH_FILE_3 "/folder3/file3.txt"
#define CONTENT_FILE_3 "ciao dal file 3 - test"
#define CLIENT_ID_1 1001
#define CLIENT_ID_2 1002

#define PRINT_FS_STATS(FS, E)                                              \
    printf("NUM OF FILES: %d\n", ResultFile_getCurrentNumOfFiles(FS, &E)); \
    printf("SIZE: %d\n", ResultFile_getCurrentSizeInByte(FS, &E));

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

    // clients' ids
    OwnerId client_1, client_2;
    client_1.id = CLIENT_ID_1;
    client_2.id = CLIENT_ID_2;

    FileSystem fs = FileSystem_create(MAX_STORAGE_SIZE, MAX_NUM_OF_FILES, USED_POLICY, &error);

    ResultFile rf = NULL;

    FileSystem_openFile(fs, PATH_FILE_1, O_CREATE | O_LOCK, client_1, &error);
    FileSystem_appendToFile(fs, PATH_FILE_1, CONTENT_FILE_1, sizeof(CONTENT_FILE_1) + 1, client_1, 0, &error);

    FileSystem_openFile(fs, PATH_FILE_2, O_CREATE | O_LOCK, client_2, &error);
    FileSystem_appendToFile(fs, PATH_FILE_2, CONTENT_FILE_2, sizeof(CONTENT_FILE_2) + 1, client_2, 0, &error);

    FileSystem_openFile(fs, PATH_FILE_3, O_CREATE | O_LOCK, client_1, &error);
    FileSystem_appendToFile(fs, PATH_FILE_3, CONTENT_FILE_3, sizeof(CONTENT_FILE_3) + 1, client_1, 0, &error);

    printf("files size: %d\n", sizeof(CONTENT_FILE_1) + 1 + sizeof(CONTENT_FILE_2) + 1 + sizeof(CONTENT_FILE_3) + 1);
    PRINT_FS_STATS(fs, error);

    FileSystem_delete(&fs, &error);

    printf("Error: %d\n", error);

    puts("well done!");
}