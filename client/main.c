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
#include "api.h"

// ABORT_ABRUPTLY_IF_NON_ZERO
#define AAINZ(code, message) \
  if (code != 0)             \
  {                          \
    perror(message);         \
    exit(EXIT_FAILURE);      \
  }

int main(int argc, char **argv)
{
  char *sockname = argv[1]; // TODO da passare da linea di comando a modino

  struct timespec abstime;
  abstime.tv_sec = 10;
  abstime.tv_nsec = 0;

  AAINZ(openConnection(sockname, 100, abstime), "openConnection has failed")


  AAINZ(openFile("/test.txt", 2), "openFile has failed")


  AAINZ(closeConnection(sockname), "closeConnection has failed")
}
