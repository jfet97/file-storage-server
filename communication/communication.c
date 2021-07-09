#include "communication.h"

/* Read "n" bytes from a descriptor */
ssize_t readn(int fd, void *v_ptr, size_t n)
{
  char *ptr = v_ptr;
  size_t nleft;
  ssize_t nread;

  nleft = n;
  while (nleft > 0)
  {
    if ((nread = read(fd, ptr, nleft)) < 0)
    {
      if (nleft == n)
        return -1; /* error, return -1 */
      else
        break; /* error, return amount read so far */
    }
    else if (nread == 0)
    {
      break; /* EOF */
    }
    nleft -= nread;
    ptr += nread;
  }
  return (n - nleft); /* return >= 0 */
}

/* Write "n" bytes to a descriptor */
ssize_t writen(int fd, void *v_ptr, size_t n)
{
  char *ptr = v_ptr;
  size_t nleft;
  ssize_t nwritten;

  nleft = n;
  while (nleft > 0)
  {
    if ((nwritten = write(fd, ptr, nleft)) < 0)
    {
      if (nleft == n)
        return -1; /* error, return -1 */
      else
        break; /* error, return amount written so far */
    }
    else if (nwritten == 0)
      break;
    nleft -= nwritten;
    ptr += nwritten;
  }
  return (n - nleft); /* return >= 0 */
}

// if alloc == 0, dest is considered the address where to write the read data
// if alloc == 1, dest is considered the address of a pointer that must be setted to the address of the read data
// readSize is used to return the size of the read data
int getData(int fd, void *dest, size_t *sizePtr, int alloc)
{
  size_t size = 0;

  // control flow flags
  int sizeRead = 0;
  int error = 0;
  int done = 0;

  // read the size
  HANDLE_WRNS(readn(fd, &size, sizeof(size)), sizeof(size), sizeRead = 1;, error = 1;)

  if (sizeRead)
  {
    // default behaviour: write to dest
    void *writeTo = dest;

    if (alloc)
    {
      // in this situation dest is considered as the address
      // of a pointer that we have to set to the read data
      char **destPtr = dest;

      // malloc enough space
      *destPtr = malloc(sizeof(**destPtr) * size);

      // we have to write into the allocated space
      writeTo = *destPtr;
    }

    // read the data if writeTo is not NULL
    if (writeTo)
    {
      HANDLE_WRNS(readn(fd, writeTo, size), size, done = 1;, error = 1;)
    }
    else
    {
      error = 1;
    }
  }

  if (done)
  {
    // return the size as well
    sizePtr ? *sizePtr = size : 0;
    return 0;
  }

  if (error)
  {
    perror("getData has failed");
    return -1;
  }

  return -1;
}

int sendRequestType(int fd, int request)
{
  HANDLE_WRNS(writen(fd, &request, sizeof(request)), sizeof(request), return 0;, return -1;)
}

int sendData(int fd, const void *data, size_t size)
{
  HANDLE_WRNS(writen(fd, &size, sizeof(size)), sizeof(size),
              HANDLE_WRNS(writen(fd, (void *)data, size), size, return 0;, return -1;),
              return -1;)
}

const char *fromRequestToString(int request)
{
  switch (request)
  {
  case OPEN_FILE:
    return "OPEN_FILE";
  case READ_FILE:
    return "READ_FILE";
  case WRITE_FILE:
    return "WRITE_FILE";
  case APPEND_TO_FILE:
    return "APPEND_TO_FILE";
  case READ_N_FILES:
    return "READ_N_FILES";
  case CLOSE_FILE:
    return "CLOSE_FILE";
  case REMOVE_FILE:
    return "REMOVE_FILE";
  case LOCK_FILE:
    return "LOCK_FILE";
  case UNLOCK_FILE:
    return "UNLOCK_FILE";
  default:
    return "";
  }
}