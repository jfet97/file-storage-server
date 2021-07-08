#include <stddef.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
	#include <unistd.h>


#define OPEN_FILE 10000
#define READ_FILE 10001

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
int sendRequestType(int fd, int request);
int sendData(int fd, void *data, size_t size);