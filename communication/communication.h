#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define OPEN_FILE 10000
#define READ_FILE 10001
#define WRITE_FILE 10002
#define APPEND_TO_FILE 10003
#define READ_N_FILES 10004
#define CLOSE_FILE 10005
#define REMOVE_FILE 10006
#define LOCK_FILE 10007
#define UNLOCK_FILE 10008

#define HANDLE_WRN(A, S, OK, NE, IZ, IE, D) \
  errno = 0;                                \
  int r = A;                                \
  if (r == S)                               \
  {                                         \
    OK                                      \
  }                                         \
  else if (r == 0)                          \
  {                                         \
    IZ                                      \
  }                                         \
  else if (r == -1)                         \
  {                                         \
    IE                                      \
  }                                         \
  else if (r < S)                           \
  {                                         \
    NE                                      \
  }                                         \
  else                                      \
  {                                         \
    D                                       \
  }

#define HANDLE_WRNS(A, S, OK, KO) \
  HANDLE_WRN(A, S, OK, KO, KO, KO, KO)

ssize_t readn(int fd, void *v_ptr, size_t n);
ssize_t writen(int fd, void *v_ptr, size_t n);
int getData(int fd, void *dest, size_t *readSize, int alloc);
int sendRequestType(int fd, size_t request);
int sendData(int fd, const void *data, size_t size);
const char* fromRequestToString(int request);