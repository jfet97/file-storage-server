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
#define MAX_STORAGE_SIZE 128
#define MAX_NUM_OF_FILES 3
#define PATH_FILE_1 "/folder1/file1.txt"
#define CONTENT_FILE_1 "1234567890" // 11
#define PATH_FILE_2 "/folder2/file2.txt"
#define CONTENT_FILE_2 "2345678901"                                                                                                          // 11
#define LONG_CONTENT_FILE_2 "23456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901" // 111
#define PATH_FILE_3 "/folder3/file3.txt"
#define CONTENT_FILE_3 "3456789012" // 11
#define PATH_FILE_4 "/folder4/file4.txt"
#define CONTENT_FILE_4 "4567890123" // 11
#define CLIENT_ID_1 1001
#define CLIENT_ID_2 1002

#define PRINT_FS_STATS(FS, E)                                              \
    puts("-----------------------------------\nSTART FS STATS\n");         \
    printf("NUM OF FILES: %d\n", ResultFile_getCurrentNumOfFiles(FS, &E)); \
    printf("SIZE: %d\n", ResultFile_getCurrentSizeInByte(FS, &E));         \
    {                                                                      \
        Paths ps = ResultFile_getStoredFilesPaths(FS, &E);                 \
        if (ps)                                                            \
        {                                                                  \
            puts("PATHS:");                                                \
            char **runner = ps->paths;                                     \
            while (*runner)                                                \
            {                                                              \
                puts(*runner++);                                           \
            }                                                              \
        }                                                                  \
        free(ps->paths);                                                   \
        free(ps);                                                          \
    }                                                                      \
    puts("\nEND FS STATS\n-----------------------------------\n");

#define IGNORE_APPEND_RES(A, R, E) \
    R = A;                         \
    if (R)                         \
    {                              \
        List_free(&R, 1, E);       \
    }

void print(int *error)
{
    if (error && *error)
    {
        puts(FileSystem_getErrorMessage(*error));
        *error = 0;
    }
}

// TODO: test dei flag
// TODO: test delle politiche di replacement sia quando non c'è più spazio, sia quando si ha raggiunto il massimo numero di file
// TODO: commentare il file-system, check delle condizioni nei cicli, check degli errori della lista

int main(void)
{

    int error;

    // clients' ids
    OwnerId client_1, client_2;
    client_1.id = CLIENT_ID_1;
    client_2.id = CLIENT_ID_2;

    FileSystem fs = FileSystem_create(MAX_STORAGE_SIZE, MAX_NUM_OF_FILES, USED_POLICY, &error);

    ResultFile rf = NULL;
    List_T rfs = NULL;

    FileSystem_openFile(fs, PATH_FILE_1, O_CREATE | O_LOCK, client_1, &error);
    IGNORE_APPEND_RES(FileSystem_appendToFile(fs, PATH_FILE_1, CONTENT_FILE_1, strlen(CONTENT_FILE_1), client_1, 1, &error), rfs, &error);

    puts("next should fail");
    IGNORE_APPEND_RES(FileSystem_appendToFile(fs, PATH_FILE_1, CONTENT_FILE_1, strlen(CONTENT_FILE_1) + 1, client_1, 1, &error), rfs, &error); // should fail
    print(&error);
    IGNORE_APPEND_RES(FileSystem_appendToFile(fs, PATH_FILE_1, CONTENT_FILE_1, strlen(CONTENT_FILE_1) + 1, client_1, 0, &error), rfs, &error);

    FileSystem_openFile(fs, PATH_FILE_2, O_CREATE | O_LOCK, client_2, &error);
    IGNORE_APPEND_RES(FileSystem_appendToFile(fs, PATH_FILE_2, CONTENT_FILE_2, strlen(CONTENT_FILE_2), client_2, 1, &error), rfs, &error);

    FileSystem_openFile(fs, PATH_FILE_3, O_CREATE | O_LOCK, client_1, &error);
    IGNORE_APPEND_RES(FileSystem_appendToFile(fs, PATH_FILE_3, CONTENT_FILE_3, strlen(CONTENT_FILE_3) + 1, client_1, 1, &error), rfs, &error);

    PRINT_FS_STATS(fs, error);

    // FIFO: PATH_FILE_1 will be evicted
    rf = FileSystem_openFile(fs, PATH_FILE_4, O_CREATE | O_LOCK, client_2, &error);
    IGNORE_APPEND_RES(FileSystem_appendToFile(fs, PATH_FILE_4, CONTENT_FILE_4, strlen(CONTENT_FILE_4) + 1, client_2, 1, &error), rfs, &error);
    puts("next should fail");
    IGNORE_APPEND_RES(FileSystem_appendToFile(fs, PATH_FILE_4, CONTENT_FILE_4, strlen(CONTENT_FILE_4) + 1, client_1, 1, &error), rfs, &error); // should fail
    print(&error);

    // puts(rf->path);
    // puts(rf->data);
    ResultFile_free(&rf, &error);
    PRINT_FS_STATS(fs, error);

    // FIFO: PATH_FILE_4, PATH_FILE_3, will be evicted
    IGNORE_APPEND_RES(FileSystem_appendToFile(fs, PATH_FILE_2, LONG_CONTENT_FILE_2, strlen(LONG_CONTENT_FILE_2) + 1, client_2, 0, &error), rfs, &error);
    PRINT_FS_STATS(fs, error);

    FileSystem_delete(&fs, &error);

    printf("Error: %d\n", error);

    puts("well done!");
}