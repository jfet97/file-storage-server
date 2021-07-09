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

// ABORT_ABRUPTLY_IF_NEGATIVE_ONE
#define AAINO(code, message) \
  if (code == -1)            \
  {                          \
    perror(message);         \
    exit(EXIT_FAILURE);      \
  }

#define BIG_TEXT "AAINZ(appendToFile(\"./ bin / test4.txt \", \" \", , homeDirEvictedFiles), \" openFile has failed \");"
int main(int argc, char **argv)
{
  char *sockname = "../server/mysocket"; // TODO da passare da linea di comando a modino

  struct timespec abstime;
  abstime.tv_sec = 10;
  abstime.tv_nsec = 0;

  homeDirEvictedFiles = "./bin/evicted";

  const char *homeDirReadFiles = "./bin/read";

  AAINZ(openConnection(sockname, 100, abstime), "openConnection has failed")

  // ------------------------------------------------------------------------------
  // AAINZ(openFile("./bin/test2.txt", 1), "openFile has failed")
  // AAINZ(openFile("./bin/test2.txt", 0), "openFile has failed")
  // AAINZ(openFile("./bin/test2.txt", 1), "openFile has failed")
  // AAINZ(openFile("./bin/test1.txt", 3), "openFile has failed")
  // AAINZ(openFile("./bin/test1.txt", 0), "openFile has failed")
  // AAINZ(openFile("./bin/test1.txt", 1), "openFile has failed")
  // AAINZ(openFile("./bin/test1.txt", 2), "openFile has failed")

  // char *buf = NULL;
  // size_t bufLen = 0;
  // AAINZ(readFile("./bin/test2.txt", (void**)&buf, &bufLen), "readFile has failed")
  // if (buf && bufLen)
  // {
  //   printf("HO LEGGIUTO: %.*s\n", bufLen, buf);
  // }
  // ------------------------------------------------------------------------------

  AAINZ(openFile("./bin/test1.txt", 3), "openFile has failed")
  AAINZ(openFile("./bin/test2.txt", 3), "openFile has failed")
  AAINZ(openFile("./bin/test3.txt", 3), "openFile has failed")
  AAINZ(writeFile("./bin/test1.txt", homeDirEvictedFiles), "writeFile has failed")
  AAINZ(writeFile("./bin/test2.txt", homeDirEvictedFiles), "writeFile has failed")
  AAINZ(writeFile("./bin/test3.txt", homeDirEvictedFiles), "writeFile has failed")
  AAINZ(openFile("./bin/test4.txt", 3), "openFile has failed")
  AAINO(readNFiles(0, homeDirReadFiles), "readNFiles has failed")
  AAINZ(appendToFile("./bin/test4.txt", BIG_TEXT, strlen(BIG_TEXT), homeDirEvictedFiles), "openFile has failed")
  AAINO(readNFiles(0, homeDirReadFiles), "readNFiles has failed")
  AAINZ(openFile("./bin/test5.txt", 3), "openFile has failed")
  AAINZ(writeFile("./bin/test5.txt", homeDirEvictedFiles), "writeFile has failed")
  AAINO(readNFiles(0, homeDirReadFiles), "readNFiles has failed")
  //
  // ------------------------------------------------------------------------------

  // AAINZ(writeFile(filepath, "DIRNAME"), "writeFile has failed")
  // AAINZ(appendToFile(filepath, "ciao ciao", strlen("ciao ciao"), "DIRNAME"), "appendToFile has failed")

  AAINZ(closeConnection(sockname), "closeConnection has failed")
}
