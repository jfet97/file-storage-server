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

#define N_OF_CLIENTS 150
#define USED_POLICY FS_REPLACEMENT_FIFO
// #define USED_POLICY FS_REPLACEMENT_LRU
#define MAX_STORAGE_SIZE 15000
#define MAX_NUM_OF_FILES 100
#define PATH_LENGHT 50
#define TEXT_LENGHT 1000
#define CYCLES 300

#define ec(s, r, m)     \
  if ((s) == (r))       \
  {                     \
    perror(m);          \
    exit(EXIT_FAILURE); \
  }

#define ec_n(s, r, m)   \
  if ((s) != (r))       \
  {                     \
    perror(m);          \
    exit(EXIT_FAILURE); \
  }

#define PRINT_RESULTING_FILE(F)     \
  puts("------------------------"); \
  puts("Resulting file:");          \
  puts(F->path);                    \
  puts(F->data);                    \
  puts("------------------------");

#define IF_EXISTS_PRINT_FREE_RES_FILE(F) \
  if (F)                                 \
  {                                      \
    PRINT_RESULTING_FILE(F);             \
    ResultFile_free(&F, NULL);           \
  }

#define PRINT_ERROR(E) \
  if (*E)              \
    printError(E);

#define PRINT_QUEUE_OF_RES_FILE(A, R, E) \
  R = A;                                 \
  if (R)                                 \
  {                                      \
    List_forEach(R, printResultFile, E); \
    List_free(&R, 1, E);                 \
  }

#define IGNORE_QUEUE_OF_RES_FILE(A, R, E) \
  R = A;                                  \
  if (R)                                  \
  {                                       \
    List_free(&R, 1, E);                  \
  }

typedef struct
{
  char path[PATH_LENGHT];
} FileMT;

typedef struct
{
  FileMT *files;
  FileSystem fs;
  int id;
} WorkerContext;

void printResultFile(void *rawFile, int *_)
{
  ResultFile f = rawFile;
  PRINT_RESULTING_FILE(f)
}

static char *rand_string(char *str, size_t size)
{
  static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJK...";
  if (size)
  {
    --size; // to place the null terminator
    for (size_t n = 0; n < size; n++)
    {
      int key = rand() % (int)(sizeof charset - 1);
      str[n] = charset[key];
    }
    str[size] = '\0';
  }
  return str;
}

void printError(int *error)
{
  if (error && *error)
  {
    puts(FileSystem_getErrorMessage(*error));
    *error = 0;
  }
}

void *worker(void *arg)
{

  WorkerContext *ctx = arg;

  OwnerId id;
  id.id = ctx->id;
  int file = rand() % MAX_NUM_OF_FILES;
  int action = rand() % 6;
  int error = 0;

  ResultFile rf;
  List_T rfs;

  for (int i = 0; i < CYCLES; i++)
  {
    switch (action)
    {
    case 0:
    {
      rf = FileSystem_openFile(ctx->fs, ctx->files[file].path, 0, id, &error);
      if (error)
      {
        // PRINT_ERROR(&error);
      }
      else
      {
        // IF_EXISTS_PRINT_FREE_RES_FILE(rf);
        char text[TEXT_LENGHT];
        rand_string(text, TEXT_LENGHT);

        IGNORE_QUEUE_OF_RES_FILE(FileSystem_appendToFile(ctx->fs, ctx->files[file].path, text, TEXT_LENGHT, id, 0, &error);, rfs, &error)
        // PRINT_ERROR(&error);
        FileSystem_closeFile(ctx->fs, ctx->files[file].path, id, &error);
        // PRINT_ERROR(&error);
      }
      break;
    }
    case 1:
    {
      rf = FileSystem_openFile(ctx->fs, ctx->files[file].path, O_CREATE, id, &error);
      if (error)
      {
        // PRINT_ERROR(&error);
      }
      else
      {
        // IF_EXISTS_PRINT_FREE_RES_FILE(rf);
        char text[TEXT_LENGHT];
        rand_string(text, TEXT_LENGHT);

        IGNORE_QUEUE_OF_RES_FILE(FileSystem_appendToFile(ctx->fs, ctx->files[file].path, text, TEXT_LENGHT, id, 0, &error);, rfs, &error)
        // PRINT_ERROR(&error);
        FileSystem_closeFile(ctx->fs, ctx->files[file].path, id, &error);
        // PRINT_ERROR(&error);
      }
      break;
    }

    // case 1: //readN
    //   readNfiles(10, &list, c);
    //   if (list)
    //   {
    //     queueDestroy(list);
    //     list = NULL;
    //   }
    //   break;
    // case 2: //lock
    //   LoggerLog("BEGIN-rt2", strlen("BEGIN-rt2"));
    //   lockFile(f, c);
    //   lockFile(rFile(), c);
    //   lockFile(rFile(), c);
    //   unlockFile(f, c);
    //   unlockFile(rFile(), c); //NOTE:
    //   unlockFile(rFile(), c); // These two aren't the ones above
    //   LoggerLog("END-rt2", strlen("END-rt2"));
    //   break;
    // case 3:
    //   openFile(f, 1, 1, c, NULL);
    //   appendToFile(f, gen_random(msg, MSGLEN), MSGLEN, c, &list, 1);
    //   if (list)
    //   {
    //     queueDestroy(list);
    //     list = NULL;
    //   }
    //   closeFile(f, c);
    //   break;
    // case 4: //remove
    //   removeFile(f, c, &victim);
    //   if (victim)
    //   {
    //     freeEvicted(victim);
    //     victim = NULL;
    //   }
    //   break;
    // case 5: //removeClient
    //   queue *notify = NULL;
    //   notify = storeRemoveClient(c);
    //   strerror_r(errno, buf, 200);
    //   if (!notify)
    //   {
    //     perror(ANSI_COLOR_CYAN "INTERNAL FATAL ERROR" ANSI_COLOR_RESET);
    //     exit(EXIT_FAILURE);
    //   }
    //   res = 0;
    //   sprintf(ret, "%d__%d__storeRemoveClient__%s", t, res, buf);
    //   puts(ret);
    //   queueDestroy(notify);
    default:
    {
      break;
    }
    }
  }

  return NULL;
}

int main(void)
{

  int err;
  srand(time(NULL));

  // INIT FS
  FileSystem fs = FileSystem_create(MAX_STORAGE_SIZE, MAX_NUM_OF_FILES / 2, USED_POLICY, &err);

  // INIT FILES
  FileMT files[MAX_NUM_OF_FILES];
  for (int i = 0; i < MAX_NUM_OF_FILES; i++)
  {
    rand_string(files[i].path, PATH_LENGHT);
  }

  // INIT CONTEXTS
  WorkerContext ctxs[N_OF_CLIENTS];
  for (int i = 0; i < N_OF_CLIENTS; i++)
  {
    ctxs[i].id = i;
    ctxs[i].fs = fs;
    ctxs[i].files = files;
  }

  // LAUNCH WORKERS
  pthread_t workers[N_OF_CLIENTS];
  for (int i = 0; i < N_OF_CLIENTS; i++)
  {
    ec_n((err = pthread_create(&(workers[i]), NULL, worker, (void *)(ctxs + i))), 0, "worker");
  }

  // JOIN WORKERS
  for (int i = 0; i < N_OF_CLIENTS; ++i)
  {
    pthread_join(workers[i], NULL);
  }

  FileSystem_delete(&fs, &err);
  PRINT_ERROR(&err);
}
