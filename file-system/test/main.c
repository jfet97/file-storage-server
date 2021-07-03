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
#define MAX_STORAGE_SIZE 130
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
#define SMALL_TEXT "123"            // 4
#define VOID_TEXT ""                // 1
#define CLIENT_ID_1 1001
#define CLIENT_ID_2 1002
#define CLIENT_ID_3 1003
#define CLIENT_ID_4 1004
#define CLIENT_ID_5 1005

#define PRINT_RESULTING_FILE(F)       \
    puts("------------------------"); \
    puts("Resulting file:");          \
    puts(F->path);                    \
    puts(F->data);                    \
    puts("------------------------");

#define PRINT_OWNER_ID(O)             \
    puts("------------------------"); \
    puts("Owner Id:");                \
    printf("%d\n", O->id);                       \
    puts("------------------------");

void printResultFile(void *rawFile, int *_)
{
    ResultFile f = rawFile;
    PRINT_RESULTING_FILE(f)
}

void printOids(void *rawOid, int *_)
{
    OwnerId *oid = rawOid;
    PRINT_OWNER_ID(oid);
}

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

#define IGNORE_QUEUE_OF_RES_FILE(A, R, E) \
    R = A;                                \
    if (R)                                \
    {                                     \
        List_free(&R, 1, E);              \
    }

#define PRINT_QUEUE_OF_RES_FILE(A, R, E)     \
    R = A;                                   \
    if (R)                                   \
    {                                        \
        List_forEach(R, printResultFile, E); \
        List_free(&R, 1, E);                 \
    }

#define PRINT_QUEUE_OF_OWNIDS(Q, E)           \
    {                                         \
        List_T oids = Q;                      \
        if (oids)                             \
        {                                     \
            List_forEach(oids, printOids, E); \
            List_free(&oids, 1, E);           \
        }                                     \
    }

#define SHOULD_FAIL(C)        \
    puts("next should fail"); \
    {                         \
        C                     \
            print(&error);    \
    };

void print(int *error)
{
    if (error && *error)
    {
        puts(FileSystem_getErrorMessage(*error));
        *error = 0;
    }
}

// TODO: test delle politiche di replacement sia quando non c'è più spazio, sia quando si ha raggiunto il massimo numero di file
// TODO: aggiungi statistiche nel file-system e aggiorna al tempo opportuno
// TODO: hai fatto le signal?

// invarianti:
// non posso aprire un file lockato da qualcun'altro
// posso aprire/chiudere un file non lockato
// posso lockare/unlockare un file che non ho aperto
// non posso lockare immediatamente un file lockato da altri, mi metto in attesa
// non posso leggere un file lockato da un altro
// non posso rimuovere un file che non ho lockato
// posso chiudere un file lockato da altri

int main(void)
{

    int error;

    // clients' ids
    OwnerId client_1, client_2, client_3, client_4, client_5;
    client_1.id = CLIENT_ID_1;
    client_2.id = CLIENT_ID_2;
    client_3.id = CLIENT_ID_3;
    client_4.id = CLIENT_ID_4;
    client_5.id = CLIENT_ID_5;

    FileSystem fs = FileSystem_create(MAX_STORAGE_SIZE, MAX_NUM_OF_FILES, USED_POLICY, &error);

    ResultFile rf = NULL;
    List_T rfs = NULL;
    OwnerId *oid = NULL;

    // ---------------------------------------------------------------------------------------------------------------------------------------------

    FileSystem_openFile(fs, PATH_FILE_1, O_CREATE | O_LOCK, client_1, &error);
    IGNORE_QUEUE_OF_RES_FILE(FileSystem_appendToFile(fs, PATH_FILE_1, CONTENT_FILE_1, strlen(CONTENT_FILE_1), client_1, 1, &error), rfs, &error);
    SHOULD_FAIL(
        IGNORE_QUEUE_OF_RES_FILE(FileSystem_appendToFile(fs, PATH_FILE_1, CONTENT_FILE_1, strlen(CONTENT_FILE_1) + 1, client_1, 1, &error), rfs, &error);)
    IGNORE_QUEUE_OF_RES_FILE(FileSystem_appendToFile(fs, PATH_FILE_1, CONTENT_FILE_1, strlen(CONTENT_FILE_1) + 1, client_1, 0, &error), rfs, &error);
    FileSystem_openFile(fs, PATH_FILE_2, O_CREATE | O_LOCK, client_2, &error);
    IGNORE_QUEUE_OF_RES_FILE(FileSystem_appendToFile(fs, PATH_FILE_2, CONTENT_FILE_2, strlen(CONTENT_FILE_2), client_2, 1, &error), rfs, &error);
    FileSystem_openFile(fs, PATH_FILE_3, O_CREATE | O_LOCK, client_1, &error);
    IGNORE_QUEUE_OF_RES_FILE(FileSystem_appendToFile(fs, PATH_FILE_3, CONTENT_FILE_3, strlen(CONTENT_FILE_3) + 1, client_1, 1, &error), rfs, &error);
    PRINT_FS_STATS(fs, error);

    // ---------------------------------------------------------------------------------------------------------------------------------------------

    // FIFO: PATH_FILE_1 will be evicted
    rf = FileSystem_openFile(fs, PATH_FILE_4, O_CREATE | O_LOCK, client_2, &error);
    PRINT_RESULTING_FILE(rf)
    IGNORE_QUEUE_OF_RES_FILE(FileSystem_appendToFile(fs, PATH_FILE_4, CONTENT_FILE_4, strlen(CONTENT_FILE_4) + 1, client_2, 1, &error), rfs, &error);
    SHOULD_FAIL(
        IGNORE_QUEUE_OF_RES_FILE(FileSystem_appendToFile(fs, PATH_FILE_4, CONTENT_FILE_4, strlen(CONTENT_FILE_4) + 1, client_1, 1, &error), rfs, &error);)
    ResultFile_free(&rf, &error);
    PRINT_FS_STATS(fs, error);

    // ---------------------------------------------------------------------------------------------------------------------------------------------

    // FIFO: PATH_FILE_4, PATH_FILE_3, will be evicted
    PRINT_QUEUE_OF_RES_FILE(FileSystem_appendToFile(fs, PATH_FILE_2, LONG_CONTENT_FILE_2, strlen(LONG_CONTENT_FILE_2), client_2, 0, &error), rfs, &error);
    PRINT_FS_STATS(fs, error);

    // ---------------------------------------------------------------------------------------------------------------------------------------------

    FileSystem_unlockFile(fs, PATH_FILE_2, client_2, &error);
    FileSystem_openFile(fs, PATH_FILE_2, 0, client_1, &error);
    IGNORE_QUEUE_OF_RES_FILE(FileSystem_appendToFile(fs, PATH_FILE_2, SMALL_TEXT, strlen(SMALL_TEXT), client_1, 0, &error), rfs, &error);
    PRINT_FS_STATS(fs, error);

    // ---------------------------------------------------------------------------------------------------------------------------------------------

    FileSystem_lockFile(fs, PATH_FILE_2, client_1, &error);
    SHOULD_FAIL(
        IGNORE_QUEUE_OF_RES_FILE(FileSystem_appendToFile(fs, PATH_FILE_2, SMALL_TEXT, strlen(SMALL_TEXT) + 1, client_2, 0, &error), rfs, &error);)
    SHOULD_FAIL(
        FileSystem_lockFile(fs, PATH_FILE_2, client_1, &error);)
    FileSystem_lockFile(fs, PATH_FILE_2, client_2, &error); // should fail successfully
    print(&error);
    oid = FileSystem_unlockFile(fs, PATH_FILE_2, client_1, &error);
    printf("%d should be %d\n", client_2.id, oid->id);
    free(oid);
    PRINT_FS_STATS(fs, error);

    // ---------------------------------------------------------------------------------------------------------------------------------------------

    IGNORE_QUEUE_OF_RES_FILE(FileSystem_appendToFile(fs, PATH_FILE_2, SMALL_TEXT, strlen(SMALL_TEXT) + 1, client_2, 0, &error), rfs, &error);
    FileSystem_unlockFile(fs, PATH_FILE_2, client_2, &error);
    PRINT_FS_STATS(fs, error);

    // ---------------------------------------------------------------------------------------------------------------------------------------------

    FileSystem_closeFile(fs, PATH_FILE_2, client_1, &error);
    SHOULD_FAIL(
        IGNORE_QUEUE_OF_RES_FILE(FileSystem_appendToFile(fs, PATH_FILE_2, VOID_TEXT, strlen(VOID_TEXT) + 1, client_1, 0, &error), rfs, &error);)
    SHOULD_FAIL(
        FileSystem_openFile(fs, PATH_FILE_2, O_CREATE | O_LOCK, client_1, &error);)
    SHOULD_FAIL(
        FileSystem_openFile(fs, PATH_FILE_2, O_CREATE, client_1, &error);)
    FileSystem_openFile(fs, PATH_FILE_2, O_LOCK, client_1, &error);
    SHOULD_FAIL(
        IGNORE_QUEUE_OF_RES_FILE(FileSystem_appendToFile(fs, PATH_FILE_2, VOID_TEXT, strlen(VOID_TEXT) + 1, client_2, 0, &error), rfs, &error);)
    FileSystem_unlockFile(fs, PATH_FILE_2, client_1, &error);
    FileSystem_closeFile(fs, PATH_FILE_2, client_1, &error);
    print(&error);
    PRINT_FS_STATS(fs, error);

    // ---------------------------------------------------------------------------------------------------------------------------------------------

    rf = FileSystem_readFile(fs, PATH_FILE_2, client_2, &error);
    PRINT_RESULTING_FILE(rf)
    ResultFile_free(&rf, &error);
    PRINT_FS_STATS(fs, error);

    // ---------------------------------------------------------------------------------------------------------------------------------------------

    puts("TEST REMOVE");
    SHOULD_FAIL(
        FileSystem_removeFile(fs, PATH_FILE_2, client_2, &error);)
    FileSystem_lockFile(fs, PATH_FILE_2, client_2, &error);
    FileSystem_removeFile(fs, PATH_FILE_2, client_2, &error);
    PRINT_FS_STATS(fs, error);

    // ---------------------------------------------------------------------------------------------------------------------------------------------

    SHOULD_FAIL(FileSystem_openFile(fs, PATH_FILE_1, O_LOCK, client_1, &error);)
    SHOULD_FAIL(FileSystem_openFile(fs, PATH_FILE_1, 0, client_1, &error);)
    FileSystem_openFile(fs, PATH_FILE_1, O_CREATE, client_1, &error);
    FileSystem_openFile(fs, PATH_FILE_2, O_CREATE, client_1, &error);
    FileSystem_openFile(fs, PATH_FILE_3, O_CREATE, client_1, &error);
    SHOULD_FAIL(IGNORE_QUEUE_OF_RES_FILE(FileSystem_appendToFile(fs, PATH_FILE_1, CONTENT_FILE_1, strlen(CONTENT_FILE_1), client_1, 1, &error), rfs, &error);)
    IGNORE_QUEUE_OF_RES_FILE(FileSystem_appendToFile(fs, PATH_FILE_1, CONTENT_FILE_1, strlen(CONTENT_FILE_1) + 1, client_1, 0, &error), rfs, &error);
    IGNORE_QUEUE_OF_RES_FILE(FileSystem_appendToFile(fs, PATH_FILE_2, CONTENT_FILE_2, strlen(CONTENT_FILE_2) + 1, client_1, 0, &error), rfs, &error);
    IGNORE_QUEUE_OF_RES_FILE(FileSystem_appendToFile(fs, PATH_FILE_3, CONTENT_FILE_3, strlen(CONTENT_FILE_3) + 1, client_1, 0, &error), rfs, &error);
    PRINT_FS_STATS(fs, error);

    // ---------------------------------------------------------------------------------------------------------------------------------------------

    puts("@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
    PRINT_QUEUE_OF_RES_FILE(FileSystem_readNFile(fs, client_1, 1, &error), rfs, &error);
    puts("@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
    PRINT_QUEUE_OF_RES_FILE(FileSystem_readNFile(fs, client_1, 3, &error), rfs, &error);
    puts("@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
    PRINT_QUEUE_OF_RES_FILE(FileSystem_readNFile(fs, client_1, 5, &error), rfs, &error);
    puts("@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
    PRINT_FS_STATS(fs, error);
    FileSystem_printAll_DEBUG(fs);
    PRINT_FS_STATS(fs, error);

    // ---------------------------------------------------------------------------------------------------------------------------------------------

    FileSystem_openFile(fs, PATH_FILE_1, 0, client_2, &error);
    FileSystem_openFile(fs, PATH_FILE_2, 0, client_2, &error);
    FileSystem_openFile(fs, PATH_FILE_3, 0, client_2, &error);
    FileSystem_lockFile(fs, PATH_FILE_1, client_1, &error);
    FileSystem_lockFile(fs, PATH_FILE_2, client_2, &error);
    FileSystem_lockFile(fs, PATH_FILE_3, client_1, &error);
    FileSystem_lockFile(fs, PATH_FILE_1, client_4, &error);
    FileSystem_lockFile(fs, PATH_FILE_1, client_5, &error);
    FileSystem_lockFile(fs, PATH_FILE_2, client_3, &error);
    FileSystem_lockFile(fs, PATH_FILE_3, client_3, &error);
    FileSystem_printAll_DEBUG(fs);
    PRINT_QUEUE_OF_OWNIDS(FileSystem_evictClient(fs, client_1, &error);, &error)
    FileSystem_printAll_DEBUG(fs);
    PRINT_FS_STATS(fs, error);

    // ---------------------------------------------------------------------------------------------------------------------------------------------

    FileSystem_delete(&fs, &error);
    printf("Error: %d\n", error);

    puts("well done!");
}