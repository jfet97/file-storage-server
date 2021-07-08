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
    size_t pathLen = strlen(pathname);

    AINZ(sendRequestType(fd_skt, OPEN_FILE), "OPEN_FILE request failed", toRet = -1; toErrno = errno;)
    AINZ(sendData(fd_skt, pathname, pathLen), "OPEN_FILE request failed", toRet = -1; toErrno = errno;)
    AINZ(sendData(fd_skt, &flags, sizeof(int)), "OPEN_FILE request failed", toRet = -1; toErrno = errno;)

    break;
  }
  case READ_FILE:
  {
    char *pathname = va_arg(valist, char *);
    size_t pathLen = strlen(pathname);

    AINZ(sendRequestType(fd_skt, READ_FILE), "READ_FILE request failed", toRet = -1; toErrno = errno;)
    AINZ(sendData(fd_skt, pathname, pathLen), "READ_FILE request failed", toRet = -1; toErrno = errno;)

    break;
  }
  case WRITE_FILE:
  case APPEND_TO_FILE:
  {
    char *pathname = va_arg(valist, char *);
    size_t pathLen = strlen(pathname); // null terminator included
    void *buf = va_arg(valist, void *);
    size_t size = va_arg(valist, size_t);
    int isWrite = va_arg(valist, int);

    if (isWrite)
    {
      AINZ(sendRequestType(fd_skt, WRITE_FILE), "WRITE_FILE request failed", toRet = -1; toErrno = errno;)
    }
    else
    {
      AINZ(sendRequestType(fd_skt, APPEND_TO_FILE), "APPEND_TO_FILE request failed", toRet = -1; toErrno = errno;)
    }

    AINZ(sendData(fd_skt, pathname, pathLen), "APPEND_TO_FILE request failed", toRet = -1; toErrno = errno;)
    AINZ(sendData(fd_skt, buf, size), "APPEND_TO_FILE request failed", toRet = -1; toErrno = errno;)

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

int readLocalFile(const char *path, void **bufPtr, size_t *size)
{
  AIN(path, "invalid path argument for readLocalFile", errno = EINVAL; return -1;)
  AIN(bufPtr, "invalid bufPtr argument for readLocalFile", errno = EINVAL; return -1;)
  AIN(size, "invalid size argument for readLocalFile", errno = EINVAL; return -1;)

  FILE *fptr = NULL;
  *bufPtr = NULL;

  // control flow flags
  int error = 0;
  int closeFile = 0;
  int freeBuf = 0;

  // open the file
  fptr = fopen(path, "r");
  AIN(fptr, "cannot open the file in readLocalFile", error = 1;)

  // go to the end of the file
  if (!error)
  {
    AINZ(fseek(fptr, 0L, SEEK_END), "readLocalFile internal error: fseek", error = 1; closeFile = 1;)
  }

  // read its size
  if (!error)
  {
    *size = ftell(fptr);
    AINO(*size, "readLocalFile internal error: ftell", error = 1; closeFile = 1;)
  }

  // rewind the file pointer
  if (!error)
  {
    errno = 0;
    rewind(fptr);
    if (errno)
    {
      error = 1;
      closeFile = 1;
      perror("readLocalFile internal error: rewind");
    }
  }

  // alloc enough space
  if (!error)
  {
    *bufPtr = malloc(sizeof(char) * (*size));
    AIN(*bufPtr, "readLocalFile internal error: malloc", error = 1; closeFile = 1;)
  }

  // read the file into the buffer
  if (!error)
  {
    int readSize = fread(*bufPtr, sizeof(char), *size, fptr);
    if (readSize < *size)
    {
      perror("readLocalFile internal error: fread");
      error = 1;
      freeBuf = 1;
    }

    closeFile = 1;
  }

  if (closeFile)
  {
    errno = 0;
    fclose(fptr);
    if (errno)
    {
      perror("readLocalFile internal error: fclose");
    }
  }

  if (freeBuf)
  {
    free(*bufPtr);
    *bufPtr = NULL;
  }

  if (error)
  {
    return -1;
  }
  else
  {
    return 0;
  }
}

// ---------------- API ----------------

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

  AIN(pathname, "invalid pathname argument for readFile", errno = EINVAL; return -1;)
  // TODO: AIN(buf, "invalid buf argument for readFile", errno = EINVAL; return -1;) obbligatorio?

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

// TODO: pathname must be absolute, supportare relativi
int writeFile(const char *pathname, const char *dirname)
{
  CHECK_FD

  AIN(pathname, "invalid pathname argument for writeFile", errno = EINVAL; return -1;)

  if (strchr(pathname, '/') != pathname)
  {
    puts("invalid pathname, it must be absolute");
    errno = EINVAL;
    return -1;
  }

  void *buf = NULL;
  size_t size = 0;
  AINZ(
      readLocalFile(pathname, &buf, &size), "writeFile has failed - cannot read the file from local disk",
      {
        if (buf)
        {
          free(buf);
        }
        return -1;
      });

  AINZ(doRequest(WRITE_FILE, pathname, buf, size, 1), "writeFile has failed", return -1;)

  // TODO: gestire risposta

  return 0;
}

// TODO: pathname must be absolute, supportare relativi
int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname)
{
  CHECK_FD

  AIN(pathname, "invalid pathname argument for appendToFile", errno = EINVAL; return -1;)
  AIN(buf, "invalid buf argument for appendToFile", errno = EINVAL; return -1;)

  if (strchr(pathname, '/') != pathname)
  {
    puts("invalid pathname, it must be absolute");
    errno = EINVAL;
    return -1;
  }

  AINZ(doRequest(APPEND_TO_FILE, pathname, buf, size, 0), "appendToFile has failed", return -1;)

  // TODO: gestire risposta

  return 0;
}

int lockFile(const char *pathname);
int unlockFile(const char *pathname);
int closeFile(const char *pathname);
int removeFile(const char *pathname);
int readNFiles(int N, const char *dirname);