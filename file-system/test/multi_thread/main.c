#define _POSIX_C_SOURCE 200809L

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

#define N_OF_CLIENTS 20
// #define USED_POLICY FS_REPLACEMENT_FIFO
#define USED_POLICY FS_REPLACEMENT_LRU
#define MAX_STORAGE_SIZE 15000
#define MAX_NUM_OF_FILES 100
#define PATH_LENGTH 50
#define TEXT_LENGTH 250
#define CYCLES 30000

#ifndef SYS_gettid
#error "SYS_gettid unavailable on this system"
#endif

#define gettid() ((pid_t)syscall(SYS_gettid))

#define DEBUG

#ifdef DEBUG
#define D(x) x
#else
#define D(x)
#endif

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

#define PRINT_QUEUE_OF_OWNIDS(Q, E)        \
  {                                        \
    List_T oids = Q;                       \
    if (oids)                              \
    {                                      \
      D(List_forEach(oids, printOids, E);) \
      List_free(&oids, 1, E);              \
    }                                      \
  }

#define PRINT_RESULTING_FILE(F)            \
  puts("------------------------");        \
  puts("Resulting file:");                 \
  puts(F->path);                           \
  printf("%.*s\n", (int)F->size, F->data); \
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

#define PRINT_QUEUE_OF_RES_FILE(A, R, E)    \
  R = A;                                    \
  if (R)                                    \
  {                                         \
    D(List_forEach(R, printResultFile, E);) \
    List_free(&R, 1, E);                    \
  }

#define IGNORE_QUEUE_OF_RES_FILE(A, R, E) \
  R = A;                                  \
  if (R)                                  \
  {                                       \
    List_free(&R, 1, E);                  \
  }

#define PRINT_OWNER_ID(O)           \
  puts("------------------------"); \
  puts("Owner Id:");                \
  printf("%d\n", O->id);            \
  puts("------------------------");

typedef struct
{
  char path[PATH_LENGTH];
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

void printOids(void *rawOid, int *_)
{
  OwnerId *oid = rawOid;
  PRINT_OWNER_ID(oid);
}

static char *rand_string(char *str, size_t size, int term)
{
  static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJK.";
  if (size)
  {
    if (term)
    {
      --size; // to place the null terminator
    }
    for (size_t n = 0; n < size; n++)
    {
      int key = rand() % (int)(sizeof charset - 1);
      str[n] = charset[key];
    }
    if (term)
    {
      str[size] = '\0';
    }
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
  int error = 0;

  ResultFile rf;
  List_T rfs;

  for (int i = 0; i < CYCLES; i++)
  {
    int file = rand() % MAX_NUM_OF_FILES;
    int action = rand() % 8;

    puts("--------------------------");
    puts(ctx->files[file].path);
    printf("action %d\n", action);
    printf("id thread %d\n", gettid());
    puts("--------------------------\n");

    switch (action)
    {
    case 0:
    {
      rf = FileSystem_openFile(ctx->fs, ctx->files[file].path, 0, id, &error);
      if (error)
      {
        D(PRINT_ERROR(&error));
      }
      else
      {
        D(IF_EXISTS_PRINT_FREE_RES_FILE(rf);)
        char text[TEXT_LENGTH];
        rand_string(text, TEXT_LENGTH, 1);

        PRINT_QUEUE_OF_RES_FILE(FileSystem_appendToFile(ctx->fs, ctx->files[file].path, text, TEXT_LENGTH, id, 0, &error);, rfs, &error)
        D(PRINT_ERROR(&error);)
        FileSystem_closeFile(ctx->fs, ctx->files[file].path, id, &error);
        D(PRINT_ERROR(&error);)
      }
      break;
    }
    case 1:
    {
      rf = FileSystem_openFile(ctx->fs, ctx->files[file].path, O_CREATE, id, &error);
      if (error)
      {
        D(PRINT_ERROR(&error);)
      }
      else
      {
        D(IF_EXISTS_PRINT_FREE_RES_FILE(rf);)
        char text[TEXT_LENGTH];
        rand_string(text, TEXT_LENGTH, 1);

        PRINT_QUEUE_OF_RES_FILE(FileSystem_appendToFile(ctx->fs, ctx->files[file].path, text, TEXT_LENGTH, id, 0, &error);, rfs, &error)
        D(PRINT_ERROR(&error);)
        FileSystem_closeFile(ctx->fs, ctx->files[file].path, id, &error);
        D(PRINT_ERROR(&error);)
      }
      break;
    }
    case 2:
    {
      rf = FileSystem_openFile(ctx->fs, ctx->files[file].path, O_CREATE | O_LOCK, id, &error);
      if (error)
      {
        D(PRINT_ERROR(&error);)
      }
      else
      {
        D(IF_EXISTS_PRINT_FREE_RES_FILE(rf);)
        char text[TEXT_LENGTH];
        rand_string(text, TEXT_LENGTH, 1);

        PRINT_QUEUE_OF_RES_FILE(FileSystem_appendToFile(ctx->fs, ctx->files[file].path, text, TEXT_LENGTH, id, 0, &error);, rfs, &error)
        D(PRINT_ERROR(&error);)
        FileSystem_closeFile(ctx->fs, ctx->files[file].path, id, &error);
        D(PRINT_ERROR(&error);)
      }
      break;
    }
    case 3:
    {
      PRINT_QUEUE_OF_RES_FILE(FileSystem_readNFile(ctx->fs, id, MAX_NUM_OF_FILES / 7, &error), rfs, &error)
      D(PRINT_ERROR(&error);)
      break;
    }
    case 4:
    {
      FileSystem_lockFile(ctx->fs, ctx->files[file].path, id, &error);
      if (error)
      {
        D(PRINT_ERROR(&error));
      }
      else
      {
        struct timespec tim, tim2;
        tim.tv_sec = 0;
        tim.tv_nsec = 50000000L;
        nanosleep(&tim, &tim2);
        OwnerId *oid = FileSystem_unlockFile(ctx->fs, ctx->files[file].path, id, &error);
        if (oid)
        {
          D(printf("id lock: %d\n", oid->id);)
          free(oid);
        }
      }
      break;
    }
    case 5:
    {
      FileSystem_removeFile(ctx->fs, ctx->files[file].path, id, &error);
      D(PRINT_ERROR(&error);)
      break;
    }
    case 6:
    {
      PRINT_QUEUE_OF_OWNIDS(FileSystem_evictClient(ctx->fs, id, &error);, &error)
      D(PRINT_ERROR(&error);)
      break;
    }
    case 7:
    {
      rf = FileSystem_readFile(ctx->fs, ctx->files[file].path, id, &error);
      if (error)
      {
        D(PRINT_ERROR(&error));
      }
      else
      {
        D(IF_EXISTS_PRINT_FREE_RES_FILE(rf);)
      }
      break;
    }
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
    rand_string(files[i].path, PATH_LENGTH, 1);
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
