#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#include "api.h"

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

// ABORT_IF_ZERO
#define AIZ(code, message, action) \
  if (code == 0)                   \
  {                                \
    perror(message);               \
    action                         \
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

#define MKDIR "mkdir -p"

static int doRequest(int request, ...);
static int readLocalFile(const char *path, void **bufPtr, size_t *size);
static char *absolutify(const char *path);
static int writeLocalFile(const char *path, size_t pathLen, const void *data, size_t dataLen, char *dirname);

// ---------------- API ----------------

// if it will be provided, this variabile will contain the path of the dir
// used to store the evicted files from the server
char *homeDirEvictedFiles = NULL;

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
int openFile(const char *pathname, int flags)
{
  CHECK_FD
  int error = 0;

  // check arguments
  AIN(pathname, "invalid pathname argument for openFile", errno = EINVAL; return -1;)
  if (flags < 0 || flags > 3)
  {
    puts("invalid flags");
    errno = EINVAL;
    return -1;
  }

  char *absPath = absolutify(pathname);
  AIN(absPath, "openFile internal error", errno = EINVAL; return -1;)

  AINZ(doRequest(OPEN_FILE, absPath, flags), "openFile has failed", error = 1;)

  int resCode;
  if (!error)
  {
    // read the result code
    AINZ(getData(fd_skt, &resCode, 0, 0), "openFile has failed", error = 1;)
    printf("remote openFile has received %d as result code\n", resCode);
  }

  if (!error)
  {
    if (resCode == -1)
    {
      // read the error message
      char *errMess;
      size_t errMessLen;
      AINZ(getData(fd_skt, &errMess, &errMessLen, 1), "openFile has failed", error = 1;)

      // print the error message
      if (!error)
      {
        printf("%.*s\n", (int)errMessLen, errMess);
      }

      if (errMess)
      {
        free(errMess);
      }

      // because resCode == -1
      error = 1;
    }
    else
    {
      // read if a file was evicted
      int evicted;
      AINZ(getData(fd_skt, &evicted, 0, 0), "openFile has failed", error = 1;)

      // if a file ws evicted, get it
      if (!error && evicted)
      {
        // read the file path
        char *filepath;
        size_t filepathLen;
        AINZ(getData(fd_skt, &filepath, &filepathLen, 1), "openFile has failed", error = 1;)

        if (!error)
        {
          printf("%.*s was evicted\n", (int)filepathLen, filepath);

          // read if the file was empty
          int emptyFile;
          AINZ(getData(fd_skt, &emptyFile, 0, 0), "openFile has failed", error = 1;)

          // if it was empty, write an empty file
          if (!error && emptyFile)
          {
            puts("the file was empty");
            AINZ(writeLocalFile(filepath, filepathLen, NULL, 0, homeDirEvictedFiles), "write process in openFile to local disk has failed", ;)
          }
          // otherwise, read its content
          if (!error && !emptyFile)
          {
            // read the file content
            void *data;
            size_t dataLen;
            AINZ(getData(fd_skt, &data, &dataLen, 1), "openFile has failed", error = 1;)

            if (!error)
            {
              printf("%.*s\n", (int)dataLen, (char *)data);
              AINZ(writeLocalFile(filepath, filepathLen, data, dataLen, homeDirEvictedFiles), "write process in openFile to local disk has failed", ;)
            }

            if (data)
            {
              free(data);
            }
          }
        }

        if (filepath)
        {
          free(filepath);
        }
      }
      // if no file was evicted do nothing
      else if (!error && !evicted)
      {
        puts("no file was evicted");
      }
    }
  }

  free(absPath);
  return 0;
}

int readFile(const char *pathname, void **buf, size_t *size)
{
  CHECK_FD

  int error = 0;

  // check arguments
  AIN(pathname, "invalid pathname argument for readFile", errno = EINVAL; return -1;)
  AIN(buf, "invalid buf argument for readFile", errno = EINVAL; return -1;)

  char *absPath = absolutify(pathname);
  AIN(absPath, "readFile internal error", errno = EINVAL; return -1;)

  AINZ(doRequest(READ_FILE, absPath), "readFile has failed", error = 1;)

  int resCode;
  if (!error)
  {
    // read the result code
    AINZ(getData(fd_skt, &resCode, 0, 0), "readFile has failed", error = 1;)
    printf("remote readFile has received %d as result code\n", resCode);
  }

  if (!error && resCode == -1)
  {
    // read the error message
    char *errMess;
    size_t errMessLen;
    AINZ(getData(fd_skt, &errMess, &errMessLen, 1), "readFile has failed", error = 1;)

    // print the error message
    if (!error)
    {
      printf("%.*s\n", (int)errMessLen, errMess);
    }

    if (errMess)
    {
      free(errMess);
    }

    // because resCode == -1
    error = 1;
  }
  else if (!error && resCode != -1)
  {
    // read if the file was empty
    int emptyFile;
    AINZ(getData(fd_skt, &emptyFile, 0, 0), "readFile has failed", error = 1;)

    // if it was empty, write NULL
    if (!error && emptyFile)
    {
      puts("the file was empty");
      *size = 0;
      *buf = NULL;
    }
    // otherwise, read its content
    if (!error && !emptyFile)
    {
      AINZ(getData(fd_skt, buf, size, 1), "readFile has failed", error = 1;)
    }
  }

  free(absPath);
  return 0;
}

int writeFile(const char *pathname, const char *dirname)
{
  CHECK_FD

  // check arguments
  AIN(pathname, "invalid pathname argument for writeFile", errno = EINVAL; return -1;)

  char *absPath = absolutify(pathname);
  AIN(absPath, "writeFile internal error", errno = EINVAL; return -1;)

  void *buf = NULL;
  size_t size = 0;
  AINZ(
      readLocalFile(absPath, &buf, &size), "writeFile has failed - cannot read the file from local disk",
      {
        if (buf)
        {
          free(buf);
        }
        return -1;
      });

  AINZ(doRequest(WRITE_FILE, absPath, buf, size, 1), "writeFile has failed", return -1;)

  // TODO: gestire risposta

  free(absPath);
  return 0;
}

int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname)
{
  CHECK_FD

  // check arguments
  AIN(pathname, "invalid pathname argument for appendToFile", errno = EINVAL; return -1;)
  AIN(buf, "invalid buf argument for appendToFile", errno = EINVAL; return -1;)

  char *absPath = absolutify(pathname);
  AIN(absPath, "appendToFile internal error", errno = EINVAL; return -1;)

  AINZ(doRequest(APPEND_TO_FILE, absPath, buf, size, 0), "appendToFile has failed", return -1;)

  // TODO: gestire risposta

  free(absPath);
  return 0;
}

int lockFile(const char *pathname);
int unlockFile(const char *pathname);
int closeFile(const char *pathname);
int removeFile(const char *pathname);
int readNFiles(int N, const char *dirname);

// ---------------- Internals ----------------

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
    size_t pathLen = strlen(pathname);
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

// read a file from disk, path should be absolute
static int readLocalFile(const char *path, void **bufPtr, size_t *size)
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

// if path is a relative path, transform it into an absolute one
static char *absolutify(const char *path)
{
  AIN(path, "path argument of absolutify is NULL", return NULL;)

  char *toRet = calloc(PATH_MAX + 1, sizeof(*toRet));
  AIN(toRet, "malloc has failed in absolutify", return NULL;)

  // check if path is already absolute, in such a case clone it
  if (path == strchr(path, '/'))
  {
    memcpy(toRet, path, strlen(path) + 1);
  }
  else
  {
    // append the current work directory
    char cwd[PATH_MAX];
    AIN(getcwd(cwd, sizeof(cwd)), "getcwd has failed in absolutify", free(toRet); toRet = NULL; return NULL;)
    snprintf(toRet, PATH_MAX + 1, "%s/%s", cwd, path);
  }

  return toRet;
}

// store a file using dirname as root, if it is not null
// otherwise it has no effects
// the path must be absolute
static int writeLocalFile(const char *path, size_t pathLen, const void *data, size_t dataLen, char *dirname)
{
  if (dirname)
  {
    // arguments check
    AIN(path, "path argument of writeLocalFile is NULL", errno = EINVAL; return -1;)
    AIZ(pathLen, "pathLen argument of writeLocalFile is 0", errno = EINVAL; return -1;)
    AIZ((path[0] == '/'), "path argument of writeLocalFile is not absolute", errno = EINVAL; return -1;)

    // data == NULL || dataLen == 0 means that the file to write is empty

    // request enough space to concatenate the directory's path and the file's path
    char *finalPath = calloc(strnlen(dirname, PATH_MAX) + pathLen + 1, sizeof(*finalPath));
    AIN(finalPath, "writeLocalFile internal malloc error", return -1;)

    // concatenate those paths
    sprintf(finalPath, "%s%s", dirname, path);

    // now retrieve file's dirname
    char *slash = strrchr(finalPath, '/'); // it's the last one
    char fileNameFirstChar = slash[1];     // the file's name + its extension starts after the last slash, store its first letter
    slash[1] = '\0';                       // temporally hide the file's name
    char *dirname = strdup(finalPath);
    AIN(dirname, "writeLocalFile internal strdup error", free(finalPath); return -1;)
    slash[1] = fileNameFirstChar; // restore the first file's char

    // create file's dirname recursively
    char *createDirsShellCommand = calloc(strlen(MKDIR) + strlen(dirname) + 2, sizeof(*createDirsShellCommand));
    AIN(createDirsShellCommand, "writeLocalFile internal malloc error", free(dirname); free(finalPath); return -1;)
    sprintf(createDirsShellCommand, "%s %s", MKDIR, dirname);
    errno = 0;
    system(createDirsShellCommand);
    if (errno)
    {
      perror("writeLocalFile internal error");
    }

    // open the file to write the content
    // but only if there is data to write
    FILE *file;
    if (!errno)
    {
      file = fopen(finalPath, "w+");
      AIN(file, "writeLocalFile internal file error", ;)
    }
    if (!errno && data != NULL && dataLen != 0)
    {
      int w = fwrite(data, sizeof(char), dataLen, file);
      if (w < dataLen)
      {
        perror("writeLocalFile internal fwrite error");
      }
    }
    AINZ(fclose(file), "writeLocalFile internal close error", ;)

    free(createDirsShellCommand);
    free(finalPath);
    free(dirname);

    if (!errno)
    {
      return 0;
    }
    else
    {
      return -1;
    }
  }

  return 0;
}