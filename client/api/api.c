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
#include <ctype.h>
#include <poll.h>
#include <dirent.h>
#include <stddef.h>
#include <sys/types.h>
#include "api.h"
#include "communication.h"
#include <stdarg.h>

#define UNIX_PATH_MAX 108

#define O_CREATE 0x01
#define O_LOCK 0x02

// will be fulfilled in openConnection
char socketname[UNIX_PATH_MAX] = {0};
int fd_skt = -1;

#define CHECK_FD    \
  if (fd_skt < 0)   \
  {                 \
    errno = EBADFD; \
    return -1;      \
  }

#define CHECK_FD_SK(S)                                  \
  CHECK_FD;                                             \
  if (!S || strncmp(socketname, S, UNIX_PATH_MAX) != 0) \
  {                                                     \
    errno = EINVAL;                                     \
    return -1;                                          \
  }

// ABORT_IF_NON_ZERO
#define AINZ(code, message, action) \
  if (code != 0)                    \
  {                                 \
    perror(message);                \
    action                          \
  }

// ABORT_IF_NULL
#define AIN(code, message, action) \
  if (code == NULL)                \
  {                                \
    perror(message);               \
    action                         \
  }

// ABORT_IF_NEGATIVE_ONE
#define AINO(code, message, action) \
  if (code == -1)                   \
  {                                 \
    perror(message);                \
    action                          \
  }

static int doRequest(int request, ...)
{

  va_list valist;
  int toRet = 0;
  int toErrno = 0;

  va_start(valist, request);

  switch (request)
  {
  case OPEN_FILE:
  {
    char *pathname = va_arg(valist, char *);
    int flags = va_arg(valist, int);
    size_t pathLen = strlen(pathname) + 1; // null terminator included

    AINZ(sendRequestType(fd_skt, OPEN_FILE), "OPEN_FILE request failed", toRet = -1; toErrno = errno;)
    AINZ(sendData(fd_skt, pathname, pathLen), "OPEN_FILE request failed", toRet = -1; toErrno = errno;)
    AINZ(sendData(fd_skt, &flags, sizeof(int)), "OPEN_FILE request failed", toRet = -1; toErrno = errno;)

    break;
  }
  case READ_FILE:
  {
    char *pathname = va_arg(valist, char *);
    size_t pathLen = strlen(pathname) + 1; // null terminator included

    AINZ(sendRequestType(fd_skt, READ_FILE), "READ_FILE request failed", toRet = -1; toErrno = errno;)
    AINZ(sendData(fd_skt, pathname, pathLen), "READ_FILE request failed", toRet = -1; toErrno = errno;)

    break;
  }
  default:
  {
    break;
  }
  }

  va_end(valist);
  errno = toErrno;
  return toRet;
}

int openConnection(const char *sockname, int msec, const struct timespec abstime)
{
  // create the socket, stored globally
  fd_skt = socket(AF_UNIX, SOCK_STREAM, 0);

  // socket error control
  if (fd_skt == -1 && errno == EINTR)
  {
    perror("socket function was interrupted by a signal");
    return -1;
  }
  else if (fd_skt == -1)
  {
    perror("socket function has raised an error");
    return -1;
  }

  // set the sockaddr_un structure
  struct sockaddr_un sa;
  strncpy(sa.sun_path, sockname, UNIX_PATH_MAX);
  sa.sun_family = AF_UNIX;

  // set the interval structure to retry after msec milliseconds the connection to the server
  // in case of failure
  struct timespec interval;
  interval.tv_sec = msec / 1000;
  interval.tv_nsec = (msec % 1000) * 1000000L;

  while (connect(fd_skt, (struct sockaddr *)&sa, sizeof(sa)) == -1)
  {
    if (errno == ENOENT)
    {
      time_t curr = time(NULL);
      AINO(curr, "cannot retrieve current time", return -1;)

      if (curr >= abstime.tv_sec)
      {
        // time has expired
        errno = ETIME;
        return -1;
      }

      // wait again
      AINO(nanosleep(&interval, NULL), "nanosleep has failed", return -1;)
    }
    else
    {
      return -1;
    }
  }

  // store globally the name of the socket
  strncpy(socketname, sockname, UNIX_PATH_MAX);

  return 0;
}

int closeConnection(const char *sockname)
{
  // input and status check
  CHECK_FD_SK(sockname)

  int toRet = close(fd_skt);

  // clean up the global state
  fd_skt = -1;
  socketname[0] = '\0';

  return toRet;
}

// O_CREATE | O_LOCK for using both
// 0 for no flag
// TODO: pathname must be absolute, supportare relativi
int openFile(const char *pathname, int flags)
{
  CHECK_FD

  AIN(pathname, "invalid pathname argument for openFile", errno = EINVAL; return -1;)

  if (strchr(pathname, '/') != pathname)
  {
    puts("invalid pathname, it must be absolute");
    errno = EINVAL;
    return -1;
  }

  if (flags < 0 || flags > 3)
  {
    puts("invalid flags");
    errno = EINVAL;
    return -1;
  }

  AINZ(doRequest(OPEN_FILE, pathname, flags), "openFile has failed", return -1;)

  // TODO: gestire risposta

  return 0;
}

// TODO: pathname must be absolute, supportare relativi
int readFile(const char *pathname, void **buf, size_t *size)
{
  CHECK_FD

  AIN(pathname, "invalid pathname argument for openFile", errno = EINVAL; return -1;)

  if (strchr(pathname, '/') != pathname)
  {
    puts("invalid pathname, it must be absolute");
    errno = EINVAL;
    return -1;
  }

  AINZ(doRequest(READ_FILE, pathname), "readFile has failed", return -1;)

  // TODO: gestire risposta

  return 0;
}

int writeFile(const char *pathname, const char *dirname);
int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname);
int lockFile(const char *pathname);
int unlockFile(const char *pathname);
int closeFile(const char *pathname);
int removeFile(const char *pathname);
int readNFiles(int N, const char *dirname);