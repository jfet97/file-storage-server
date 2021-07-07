#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#include <sys/syscall.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/un.h>
#define UNIX_PATH_MAX 108
#define SOCKNAME "../server/mysocket"

#define BUF _POSIX_PIPE_BUF

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
  }                     \
  `

char *readLineFromFILE(char *buffer, unsigned int len, FILE *fp)
{
  char *res = fgets(buffer, len, fp);

  if (res == NULL)
    return res;

  return buffer;
}

void* client() {

  int fd_skt;
  struct sockaddr_un sa;

  strncpy(sa.sun_path, SOCKNAME, UNIX_PATH_MAX);
  sa.sun_family = AF_UNIX;

  fd_skt = socket(AF_UNIX, SOCK_STREAM, 0);

  while (connect(fd_skt, (struct sockaddr *)&sa, sizeof(sa)) == -1)
  {
    if (errno == ENOENT)
    {
      sleep(1);
    }
    else
    {
      perror("NIENTE CONNECT");
      exit(EXIT_FAILURE);
    }
  }

  for(int i = 0; i < 1000; i++)
  {
    char* str = "fake str";

    int ew = write(fd_skt, str, strlen(str) + 1);
    if (ew == -1)
    {
      perror("WRITE :(");
    }

    char *qwerty = calloc(_POSIX_PIPE_BUF, sizeof(*qwerty));
    int er = read(fd_skt, qwerty, _POSIX_PIPE_BUF);
    if (er == -1)
    {
      perror("READ :(");
    }

    printf("RES: %s\n", qwerty);
    free(qwerty);
  }

  close(fd_skt);

  return NULL;
}

int main(int argc, char **argv)
{

  

  pthread_t workers[100];
  for (int i = 0; i < 100; i++)
  {
    pthread_create(workers + i, NULL, client, NULL);
  }

  for (int i = 0; i < 100; i++)
  {
    pthread_join(workers[i], NULL);
  }

  // while (1)
  // {
  //   char in[1000];
  //   readLineFromFILE(in, 1000, stdin);

  //   if (strstr(in, "EOL") != NULL)
  //     break;

  //   int ew = write(fd_skt, in, strlen(in) + 1);
  //   if (ew == -1)
  //   {
  //     perror("WRITE :(");
  //   }

  //   char *qwerty = calloc(_POSIX_PIPE_BUF, sizeof(*qwerty));
  //   int er = read(fd_skt, qwerty, _POSIX_PIPE_BUF);
  //   if (er == -1)
  //   {
  //     perror("READ :(");
  //   }

  //   printf("RES: %s\n", qwerty);
  //   free(qwerty);
  // }

  
  exit(EXIT_SUCCESS);

  return 0;
}