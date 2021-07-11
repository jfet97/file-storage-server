#define _POSIX_C_SOURCE 200809L
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
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ctype.h>
#include <poll.h>
#include <dirent.h>
#include <stddef.h>
#include <sys/types.h>
#include "simple_queue.h"
#include "config-parser.h"
#include "logger.h"
#include "communication.h"
#include "file-system.h"
#include "list.h"

#define NOOP ;

#define UNIX_PATH_MAX 108

#define BUF _POSIX_PIPE_BUF

#define SOFT_QUIT 1
#define RAGE_QUIT 2

#define LOG_LEN PATH_MAX * 2

// ABORT_ABRUPTLY_IF_NON_ZERO
#define AAINZ(code, message) \
  if (code != 0)             \
  {                          \
    perror(message);         \
    exit(EXIT_FAILURE);      \
  }

// ABORT_IF_NON_ZERO
#define AINZ(code, message, action) \
  if (code != 0)                    \
  {                                 \
    perror(message);                \
    action                          \
  }

// ABORT_ABRUPTLY_IF_NULL
#define AAIN(code, message) \
  if (code == NULL)         \
  {                         \
    perror(message);        \
    exit(EXIT_FAILURE);     \
  }

#define HANDLE_QUEUE_ERROR_ABORT(E)       \
  if (E)                                  \
  {                                       \
    puts(SimpleQueue_getErrorMessage(E)); \
    exit(EXIT_FAILURE);                   \
  }

#define CLOSE(FD, S) \
  errno = 0;         \
  int c = close(FD); \
  if (c == -1)       \
  {                  \
    perror(S);       \
  }

#define FLUSH_LOGGER                   \
  {                                    \
    int e = 0;                         \
    Logger_flush(&e);                  \
    if (e)                             \
    {                                  \
      puts(Logger_getErrorMessage(e)); \
    }                                  \
  }

#define LOG(M)                         \
  {                                    \
    int e = 0;                         \
    Logger_log(M, strlen(M), &e);      \
    FLUSH_LOGGER                       \
    if (e)                             \
    {                                  \
      puts(Logger_getErrorMessage(e)); \
    }                                  \
  }

typedef struct
{
  SimpleQueue sq; // used to receive the fds from the main thread
  int pipe;       // used to send back fds to the main thread
  FileSystem fs;
} WorkerContext;

typedef struct
{
  int fd;
  int op; // the requested operation form client
} SendFilesContext;

typedef struct
{
  int pipe; // pipe with main thread
  FileSystem fs;
  int resCode;
} EvictClientCallbackContext;

// socket file name
char *SOCKNAME = NULL;

// signal handler callback
static void
sig_handler_cb(int signum, int pipe)
{
  int toSend = 0;

  char toLog[LOG_LEN] = {0};
  sprintf(toLog, "Received signal %d -", signum);
  LOG(toLog);
  FLUSH_LOGGER;

  if (signum == SIGTSTP || signum == SIGTERM)
  {
    printf("\nshutting down soon because of %d...\n", signum);
    exit(EXIT_FAILURE);
  }
  else if (signum == SIGQUIT || signum == SIGINT)
  {
    toSend = RAGE_QUIT;
  }
  else if (signum == SIGHUP)
  {
    toSend = SOFT_QUIT;
  }
  else
  {
    printf("\nUNKNOWN SIGNAL %d!\n", signum);
    exit(EXIT_FAILURE);
  }

  if (toSend)
  {
    HANDLE_WRNS(writen(pipe, &toSend, sizeof(toSend)), sizeof(toSend), NOOP, perror("failed communication with main thread");)
  }
}

// signal handler (dedicated thread)
static void *sig_handler(void *arg)
{
  sigset_t sett;
  int *pipePtr = arg;
  int pipe = *pipePtr;
  free(pipePtr);

  // initialize the POSIX signal set
  AAINZ(sigemptyset(&sett), "set of the POSIX signal set has failed")

  // add to the POSIX set the signals of interest
  AAINZ(sigaddset(&sett, SIGINT), "manipulation of POSIX signal set has failed")
  AAINZ(sigaddset(&sett, SIGQUIT), "manipulation of POSIX signal set has failed")
  AAINZ(sigaddset(&sett, SIGHUP), "manipulation of POSIX signal set has failed")
  AAINZ(sigaddset(&sett, SIGTSTP), "manipulation of POSIX signal set has failed")
  AAINZ(sigaddset(&sett, SIGTERM), "manipulation of POSIX signal set has failed")

  // apply the mask
  AAINZ(pthread_sigmask(SIG_SETMASK, &sett, NULL), "change mask of blocked signals has failed")

  int signum;

  // wait a signal
  AAINZ(sigwait(&sett, &signum), "suspension of execution of the signal handler thread has failed")

  sig_handler_cb(signum, pipe);

  return NULL;
}

static int evictClient(int, FileSystem, int);
// callback used to notify a list of fds because they may have got a lock
// or may have lost it forever
static void evictClientCallback(void *rawCtx, void *rawOid, int *error)
{
  *error = 0;
  OwnerId *idWithLock = rawOid;
  EvictClientCallbackContext *ctx = rawCtx;
  int resCode = ctx->resCode;

  // this is the id (fd) of the client waiting for the lock on the pathname file
  // we have to signal to this client what has just happened
  AINZ(sendData(idWithLock->id, &resCode, sizeof(resCode)), "cannot respond to a client", *error = -1;)

  if (resCode == -1)
  {
    // the client waits a message
    AINZ(sendData(idWithLock->id, "filesystem file has been removed", strlen("filesystem file has been removed") + 1), "cannot respond to a client", *error = -1;)
  }

  if (!*error)
  {
    // sent this fd back to the main thread
    HANDLE_WRNS(writen(ctx->pipe, &(idWithLock->id), sizeof(idWithLock->id)), sizeof(idWithLock->id), NOOP,
                {
                  perror("failed communication with main thread");
                });
  }
  else
  {
    // evict the client from the file-system
    if (evictClient(idWithLock->id, ctx->fs, ctx->pipe) == -1)
    {
      perror("error during the eviction of a client");
    }

    // close connection
    int minusOne = -1;
    HANDLE_WRNS(writen(ctx->pipe, &minusOne, sizeof(minusOne)), sizeof(minusOne), NOOP,
                {
                  perror("failed communication with main thread");
                });

    CLOSE(idWithLock->id, "cannot close the communication with a client");
  }
}
// evict a client from the file-system
static int evictClient(int fd, FileSystem fs, int pipeWithMainThread)
{
  int error = 0;
  OwnerId id;
  id.id = fd;

  char toLog[LOG_LEN];
  sprintf(toLog, "Evicting client (fd) %d -", fd);
  LOG(toLog);

  List_T lockers = FileSystem_evictClient(fs, id, &error);
  if (error)
  {
    perror(FileSystem_getErrorMessage(error));
    LOG(FileSystem_getErrorMessage(error));
  }

  if (!error && lockers)
  {

    // lockers is a list full of fds of clients that have got locks on various files
    // because of fd departure
    EvictClientCallbackContext ctx;
    ctx.fs = fs;
    ctx.pipe = pipeWithMainThread;
    ctx.resCode = 0;

    List_forEachWithContext(lockers, evictClientCallback, &ctx, &error);
  }

  if (lockers)
  {
    List_free(&lockers, 1, NULL);
    free(lockers);
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

// cleanup function
static void end()
{
  char *sockname = SOCKNAME;
  if (sockname)
  {
    // remove the socket file
    remove(sockname);
    free(SOCKNAME);
  }
  puts("Goodbye, cruel world....");
}

// create and detach the signal handler thread
static void createDetachSigHandlerThread(int pipe)
{
  pthread_t sig_handler_thread;
  int *pipePtr = malloc(sizeof(*pipePtr));
  AAIN(pipePtr, "the creation of the signal handler thread has failed")
  *pipePtr = pipe;
  AAINZ(pthread_create(&sig_handler_thread, NULL, sig_handler, pipePtr), "the creation of the signal handler thread has failed")
  AAINZ(pthread_detach(sig_handler_thread), "the detachment of the signal handler thread has failed")
}

// mask the SIGPIPE signal
static void maskSIGPIPEAndHandledSignals()
{

  // mask all signals
  sigset_t sett;
  AAINZ(sigfillset(&sett), "set of the POSIX signal set has failed");
  AAINZ(pthread_sigmask(SIG_SETMASK, &sett, NULL), "change mask of blocked signals has failed")

  // ignore SIGPIPE
  struct sigaction saa;
  memset(&saa, 0, sizeof(saa));
  saa.sa_handler = SIG_IGN;
  AAINZ(sigaction(SIGPIPE, &saa, NULL), "masking of the SIGPIPE signal has failed")

  // mask the signals handled by the dedicated signal handler thread
  AAINZ(sigemptyset(&sett), "set of the POSIX signal set has failed");
  AAINZ(sigaddset(&sett, SIGINT), "manipulation of POSIX signal set has failed")
  AAINZ(sigaddset(&sett, SIGQUIT), "manipulation of POSIX signal set has failed")
  AAINZ(sigaddset(&sett, SIGTSTP), "manipulation of POSIX signal set has failed")
  AAINZ(sigaddset(&sett, SIGTERM), "manipulation of POSIX signal set has failed")
  AAINZ(sigaddset(&sett, SIGHUP), "manipulation of POSIX signal set has failed")

  // restore other signals
  pthread_sigmask(SIG_SETMASK, &sett, NULL);
}

static int setupServerSocket(char *sockname, int upm)
{

  int fd_skt;

  // set the server socket
  struct sockaddr_un sa;
  strncpy(sa.sun_path, sockname, upm);
  sa.sun_family = AF_UNIX;
  fd_skt = socket(AF_UNIX, SOCK_STREAM, 0); // TCP socket

  // socket error control
  if (fd_skt == -1 && errno == EINTR)
  {
    perror("socket function was interrupted by a signal");
    exit(EXIT_FAILURE);
  }
  else if (fd_skt == -1)
  {
    perror("socket function has raised an error");
    exit(EXIT_FAILURE);
  }

  // bind the server socket
  int eb = bind(fd_skt, (struct sockaddr *)&sa, sizeof(sa));

  // bind error control
  if (eb == -1 && errno == EINTR)
  {
    perror("bind function was interrupted by a signal");
    exit(EXIT_FAILURE);
  }
  else if (eb == -1)
  {
    perror("bind function has raised an error");
    exit(EXIT_FAILURE);
  }

  // listen for incoming connections
  int el = listen(fd_skt, SOMAXCONN);

  // listen error control
  if (el == -1 && errno == EINTR)
  {
    perror("listen function was interrupted by a signal");
    exit(EXIT_FAILURE);
  }
  else if (el == -1)
  {
    perror("listen function has raised an error");
    exit(EXIT_FAILURE);
  }

  return fd_skt;
}

// callback to send a list of result files to a client
void sendListOfFilesCallback(void *rawCtx, void *rawRF, int *error)
{
  if (!(*error))
  {
    SendFilesContext *ctx = rawCtx;

    int fd = ctx->fd;
    int op = ctx->op;
    ResultFile rf = rawRF;

    // send the file's path
    AINZ(sendData(fd, rf->path, strlen(rf->path) + 1), "cannot respond to a client", *error = 1;)

    char toLog[LOG_LEN] = {0};
    if (op == READ_N_FILES)
    {
      sprintf(toLog, "File %s of size %d was read -", rf->path, rf->size);
    }
    if (op == WRITE_FILE || op == APPEND_TO_FILE)
    {
      sprintf(toLog, "File %s of size %d was evicted -", rf->path, rf->size);
    }
    LOG(toLog);

    // send a message to say if the file was empty or not
    // send the file's content if it is not empty
    if (rf->size)
    {
      int emptyFile = 0;
      AINZ(sendData(fd, &emptyFile, sizeof(emptyFile)), "cannot respond to a client", *error = 1;)
      AINZ(sendData(fd, rf->data, rf->size), "cannot respond to a client", *error = 1;)
    }
    else
    {
      int emptyFile = 1;
      AINZ(sendData(fd, &emptyFile, sizeof(emptyFile)), "cannot respond to a client", *error = 1;)
    }
  }
}

// worker thread for handling clients requests
static void *worker(void *args)
{
  WorkerContext *ctx = args;
  SimpleQueue sq = ctx->sq;
  FileSystem fs = ctx->fs;
  int pipe = ctx->pipe;

  int error = 0;
  int toBreak = 0;

  while (!toBreak)
  {
    // get a file descriptor
    int *fdPtr = SimpleQueue_dequeue(sq, 1, &error);
    if (error)
    {
      toBreak = 1;
    }
    else
    {
      int fd = *fdPtr;
      free(fdPtr);

      // control flow flags
      int operationHasBeenRead = 0;
      int closeConnection = 0;
      int sendBackFileDescriptor = 0;

      // read the operation
      int op = 0;
      HANDLE_WRNS(
          readn(fd, &op, sizeof(op)),
          sizeof(op),
          {
            // success
            operationHasBeenRead = 1;
          },
          {
            // whatever has appened that is not success,
            // e.g. the client has closed the connection,
            // we close the connection with the client
            if (errno)
            {
              perror("cannot read the request from the client");
            }
            closeConnection = 1;
          })

      // handle requested operation
      if (operationHasBeenRead)
      {
        OwnerId id;
        id.id = fd;

        switch (op)
        {
        case OPEN_FILE:
        {
          char *pathname = NULL;
          size_t pathLen;
          int flags;
          if (getData(fd, &pathname, &pathLen, 1) != 0)
          {
            closeConnection = 1;
            if (pathname)
            {
              free(pathname);
            }
          }
          else
          {
            if (getData(fd, &flags, NULL, 0) != 0)
            {
              closeConnection = 1;
            }
            else
            {
              char toLog[LOG_LEN] = {0};
              sprintf(toLog, "Requested OPEN_FILE operation from client %d. File %.*s with flags %d -", fd, (int)pathLen, pathname, flags);
              LOG(toLog);

              ResultFile rf = FileSystem_openFile(fs, pathname, flags, id, &error);
              int resCode = 0;
              if (error)
              {
                // in case of file-system error send a resCode of -1 and an error message
                // but do not close the connection
                // Note: a finer error handling would be possible and should be done in a real application
                resCode = -1;
                const char *mess = FileSystem_getErrorMessage(error);
                AINZ(sendData(fd, &resCode, sizeof(resCode)), "cannot respond to a client", closeConnection = 1;)
                AINZ(sendData(fd, mess, strlen(mess) + 1), "cannot respond to a client", closeConnection = 1;)
              }
              else
              {
                // in case of success send a resCode of 0
                AINZ(sendData(fd, &resCode, sizeof(resCode)), "cannot respond to a client", closeConnection = 1;)
                if (rf)
                {
                  char toLog[LOG_LEN] = {0};
                  sprintf(toLog, "File %s of size %d was evicted -", rf->path, rf->size);
                  LOG(toLog);

                  // and the evicted file, if any
                  int evicted = 1;
                  AINZ(sendData(fd, &evicted, sizeof(evicted)), "cannot respond to a client", closeConnection = 1;)
                  // send the file's path
                  AINZ(sendData(fd, rf->path, strlen(rf->path) + 1), "cannot respond to a client", closeConnection = 1;)

                  // send a message to say if the evicted file was empty or not
                  // send the file's content if it is not empty
                  if (rf->size)
                  {
                    int emptyFile = 0;
                    AINZ(sendData(fd, &emptyFile, sizeof(emptyFile)), "cannot respond to a client", closeConnection = 1;)
                    AINZ(sendData(fd, rf->data, rf->size), "cannot respond to a client", closeConnection = 1;)
                  }
                  else
                  {
                    int emptyFile = 1;
                    AINZ(sendData(fd, &emptyFile, sizeof(emptyFile)), "cannot respond to a client", closeConnection = 1;)
                  }
                  ResultFile_free(&rf, NULL);
                }
                else
                {
                  int evicted = 0;
                  AINZ(sendData(fd, &evicted, sizeof(evicted)), "cannot respond to a client", closeConnection = 1;)
                }
              }
              if (!closeConnection)
              {
                sendBackFileDescriptor = 1;
              }
            }
            free(pathname);
          }

          break;
        }
        case READ_FILE:
        {
          char *pathname = NULL;
          size_t pathLen;
          if (getData(fd, &pathname, &pathLen, 1) != 0)
          {
            closeConnection = 1;
            if (pathname)
            {
              free(pathname);
            }
          }
          else
          {
            char toLog[LOG_LEN] = {0};
            sprintf(toLog, "Requested READ_FILE operation from client %d. File %.*s -", fd, (int)pathLen, pathname);
            LOG(toLog);

            ResultFile rf = FileSystem_readFile(fs, pathname, id, &error);
            int resCode = 0;
            if (error || rf == NULL)
            {
              // in case of file-system error send a resCode of -1 and an error message
              // but do not close the connection
              // Note: a finer error handling would be possible and should be done in a real application
              resCode = -1;
              const char *mess = FileSystem_getErrorMessage(error);
              AINZ(sendData(fd, &resCode, sizeof(resCode)), "cannot respond to a client", closeConnection = 1;)
              AINZ(sendData(fd, mess, strlen(mess) + 1), "cannot respond to a client", closeConnection = 1;)
            }
            else
            {
              // in case of success send a resCode of 0
              AINZ(sendData(fd, &resCode, sizeof(resCode)), "cannot respond to a client", closeConnection = 1;)

              char toLog[LOG_LEN] = {0};
              sprintf(toLog, "File %s of size %d was read -", rf->path, rf->size);
              LOG(toLog);

              // send a message to say if the evicted file was empty or not
              // send the file's content if it is not empty

              if (rf->size)
              {
                int emptyFile = 0;
                AINZ(sendData(fd, &emptyFile, sizeof(emptyFile)), "cannot respond to a client", closeConnection = 1;)
                AINZ(sendData(fd, rf->data, rf->size), "cannot respond to a client", closeConnection = 1;)
              }
              else
              {
                int emptyFile = 1;
                AINZ(sendData(fd, &emptyFile, sizeof(emptyFile)), "cannot respond to a client", closeConnection = 1;)
              }
              ResultFile_free(&rf, NULL);
            }
            free(pathname);

            if (!closeConnection)
            {
              sendBackFileDescriptor = 1;
            }
          }
          break;
        }
        case WRITE_FILE:
        case APPEND_TO_FILE:
        {
          int isWrite = op == WRITE_FILE;

          char *pathname = NULL;
          size_t pathLen, bufLen;
          void *buf = NULL;
          if (getData(fd, &pathname, &pathLen, 1) != 0)
          {
            closeConnection = 1;
            if (pathname)
            {
              free(pathname);
            }
          }
          else
          {
            if (getData(fd, &buf, &bufLen, 1) != 0)
            {
              closeConnection = 1;
              if (buf)
              {
                free(buf);
              }
            }
            else
            {
              char toLog[LOG_LEN] = {0};
              if (isWrite)
              {
                sprintf(toLog, "Requested WRITE_FILE operation from client %d. File %.*s -", fd, (int)pathLen, pathname);
              }
              else
              {
                sprintf(toLog, "Requested APPEND_TO_FILE operation from client %d. File %.*s -", fd, (int)pathLen, pathname);
              }
              LOG(toLog);

              List_T evictedFiles = FileSystem_appendToFile(fs, pathname, buf, bufLen, id, isWrite, &error);

              int resCode = 0;
              if (error || evictedFiles == NULL)
              {
                // in case of file-system error send a resCode of -1 and an error message
                // but do not close the connection
                // Note: a finer error handling would be possible and should be done in a real application
                resCode = -1;
                const char *mess = FileSystem_getErrorMessage(error);
                AINZ(sendData(fd, &resCode, sizeof(resCode)), "cannot respond to a client", closeConnection = 1;)
                AINZ(sendData(fd, mess, strlen(mess) + 1), "cannot respond to a client", closeConnection = 1;)
              }
              else
              {
                // in case of success send a resCode of 0
                AINZ(sendData(fd, &resCode, sizeof(resCode)), "cannot respond to a client", closeConnection = 1;)

                // send num of evicted files
                int numOfEvictedFiles = List_length(evictedFiles, NULL);
                AINZ(sendData(fd, &numOfEvictedFiles, sizeof(numOfEvictedFiles)), "cannot respond to a client", closeConnection = 1;)

                // send each evicted file
                SendFilesContext ctx;
                ctx.fd = fd;
                ctx.op = op;
                List_forEachWithContext(evictedFiles, sendListOfFilesCallback, &ctx, &error);

                if (error)
                {
                  closeConnection = 1;
                }
                List_free(&evictedFiles, 1, NULL);
              }

              if (!closeConnection)
              {
                sendBackFileDescriptor = 1;
              }

              free(buf);
            }

            free(pathname);
          }

          break;
        }
        case READ_N_FILES:
        {
          int N = 0;

          if (getData(fd, &N, NULL, 0) != 0)
          {
            closeConnection = 1;
          }
          else
          {
            char toLog[LOG_LEN] = {0};
            sprintf(toLog, "Requested READ_N_FILES operation from client %d. N %d -", fd, N);
            LOG(toLog);

            List_T rfs = FileSystem_readNFile(fs, id, N, &error);

            int resCode = 0;
            if (error || rfs == NULL)
            {
              // in case of file-system error send a resCode of -1 and an error message
              // but do not close the connection
              // Note: a finer error handling would be possible and should be done in a real application
              resCode = -1;
              const char *mess = FileSystem_getErrorMessage(error);
              AINZ(sendData(fd, &resCode, sizeof(resCode)), "cannot respond to a client", closeConnection = 1;)
              AINZ(sendData(fd, mess, strlen(mess) + 1), "cannot respond to a client", closeConnection = 1;)
            }
            else
            {
              // in case of success send a resCode of 0
              AINZ(sendData(fd, &resCode, sizeof(resCode)), "cannot respond to a client", closeConnection = 1;)

              // send num of read files
              int numOfReadFiles = List_length(rfs, NULL);
              AINZ(sendData(fd, &numOfReadFiles, sizeof(numOfReadFiles)), "cannot respond to a client", closeConnection = 1;)

              // send each read file
              SendFilesContext ctx;
              ctx.fd = fd;
              ctx.op = op;
              List_forEachWithContext(rfs, sendListOfFilesCallback, &ctx, &error);

              if (error)
              {
                closeConnection = 1;
              }

              List_free(&rfs, 1, NULL);
            }

            if (!closeConnection)
            {
              sendBackFileDescriptor = 1;
            }
          }

          break;
        }
        case CLOSE_FILE:
        {
          char *pathname = NULL;
          size_t pathLen;
          if (getData(fd, &pathname, &pathLen, 1) != 0)
          {
            closeConnection = 1;
            if (pathname)
            {
              free(pathname);
            }
          }
          else
          {
            char toLog[LOG_LEN] = {0};
            sprintf(toLog, "Requested CLOSE_FILE operation from client %d. File %.*s -", fd, (int)pathLen, pathname);
            LOG(toLog);

            FileSystem_closeFile(fs, pathname, id, &error);
            int resCode = 0;
            if (error)
            {
              // in case of file-system error send a resCode of -1 and an error message
              // but do not close the connection
              // Note: a finer error handling would be possible and should be done in a real application
              resCode = -1;
              const char *mess = FileSystem_getErrorMessage(error);
              AINZ(sendData(fd, &resCode, sizeof(resCode)), "cannot respond to a client", closeConnection = 1;)
              AINZ(sendData(fd, mess, strlen(mess) + 1), "cannot respond to a client", closeConnection = 1;)
            }
            else
            {
              // in case of success send a resCode of 0
              AINZ(sendData(fd, &resCode, sizeof(resCode)), "cannot respond to a client", closeConnection = 1;)
            }
            free(pathname);

            if (!closeConnection)
            {
              sendBackFileDescriptor = 1;
            }
          }
          break;
        }
        case REMOVE_FILE:
        {
          char *pathname = NULL;
          size_t pathLen;
          if (getData(fd, &pathname, &pathLen, 1) != 0)
          {
            closeConnection = 1;
            if (pathname)
            {
              free(pathname);
            }
          }
          else
          {
            char toLog[LOG_LEN] = {0};
            sprintf(toLog, "Requested REMOVE_FILE operation from client %d. File %.*s -", fd, (int)pathLen, pathname);
            LOG(toLog);

            // if there were clients waiting for the lock on this file, we have to notify them
            List_T clientsThatWillNotGetTheLock = FileSystem_removeFile(fs, pathname, id, &error);

            int resCode = 0;
            if (error)
            {
              // in case of file-system error send a resCode of -1 and an error message
              // but do not close the connection
              // Note: a finer error handling would be possible and should be done in a real application
              resCode = -1;
              const char *mess = FileSystem_getErrorMessage(error);
              AINZ(sendData(fd, &resCode, sizeof(resCode)), "cannot respond to a client", closeConnection = 1;)
              AINZ(sendData(fd, mess, strlen(mess) + 1), "cannot respond to a client", closeConnection = 1;)
            }
            else
            {
              // in case of success send a resCode of 0
              AINZ(sendData(fd, &resCode, sizeof(resCode)), "cannot respond to a client", closeConnection = 1;)
            }
            free(pathname);

            EvictClientCallbackContext ctx;
            ctx.fs = fs;
            ctx.pipe = pipe;
            ctx.resCode = -1;

            int error = 0;
            List_forEachWithContext(clientsThatWillNotGetTheLock, evictClientCallback, &ctx, &error);
            List_free(&clientsThatWillNotGetTheLock, 1, NULL);

            if (error)
            {

              perror("error during the eviction of a client");
            }

            if (!closeConnection)
            {
              sendBackFileDescriptor = 1;
            }
          }
          break;
        }
        case LOCK_FILE:
        {
          char *pathname = NULL;
          size_t pathLen;
          if (getData(fd, &pathname, &pathLen, 1) != 0)
          {
            closeConnection = 1;
            if (pathname)
            {
              free(pathname);
            }
          }
          else
          {
            char toLog[LOG_LEN] = {0};
            sprintf(toLog, "Requested LOCK_FILE operation from client %d. File %.*s -", fd, (int)pathLen, pathname);
            LOG(toLog);

            FileSystem_lockFile(fs, pathname, id, &error);
            int resCode = 0;
            if (error == E_FS_FILE_IS_LOCKED)
            {
              // in such a case the fd has been saved internally by the file-system
              // the client will wait patiently the lock...
            }
            else if (error)
            {
              // in case of file-system error send a resCode of -1 and an error message
              // but do not close the connection
              // Note: a finer error handling would be possible and should be done in a real application
              resCode = -1;
              const char *mess = FileSystem_getErrorMessage(error);
              AINZ(sendData(fd, &resCode, sizeof(resCode)), "cannot respond to a client", closeConnection = 1;)
              AINZ(sendData(fd, mess, strlen(mess) + 1), "cannot respond to a client", closeConnection = 1;)
            }
            else
            {
              // in case of success send a resCode of 0
              AINZ(sendData(fd, &resCode, sizeof(resCode)), "cannot respond to a client", closeConnection = 1;)
            }

            free(pathname);

            if (!closeConnection && error != E_FS_FILE_IS_LOCKED)
            {
              sendBackFileDescriptor = 1;
            }
          }
          break;
        }
        case UNLOCK_FILE:
        {
          char *pathname = NULL;
          size_t pathLen;
          if (getData(fd, &pathname, &pathLen, 1) != 0)
          {
            closeConnection = 1;
            if (pathname)
            {
              free(pathname);
            }
          }
          else
          {
            char toLog[LOG_LEN] = {0};
            sprintf(toLog, "Requested UNLOCK_FILE operation from client %d. File %.*s -", fd, (int)pathLen, pathname);
            LOG(toLog);

            OwnerId *idWithLock = FileSystem_unlockFile(fs, pathname, id, &error);
            int resCode = 0;
            if (error)
            {
              // in case of file-system error send a resCode of -1 and an error message
              // but do not close the connection
              // Note: a finer error handling would be possible and should be done in a real application
              resCode = -1;
              const char *mess = FileSystem_getErrorMessage(error);
              AINZ(sendData(fd, &resCode, sizeof(resCode)), "cannot respond to a client", closeConnection = 1;)
              AINZ(sendData(fd, mess, strlen(mess) + 1), "cannot respond to a client", closeConnection = 1;)
            }
            else
            {
              // in case of success send a resCode of 0 to fd
              AINZ(sendData(fd, &resCode, sizeof(resCode)), "cannot respond to a client", closeConnection = 1;)

              if (idWithLock)
              {
                // this is the id (fd) of the client waiting for the lock on the pathname file that just got it
                // we have to signal to this client what has just happened
                int resCode = 0;
                int error = 0;
                AINZ(sendData(idWithLock->id, &resCode, sizeof(resCode)), "cannot respond to a client", error = 1;)

                if (!error)
                {
                  // sent this fd back to the main thread
                  HANDLE_WRNS(writen(pipe, &(idWithLock->id), sizeof(idWithLock->id)), sizeof(idWithLock->id), NOOP,
                              {
                                perror("failed communication with main thread");
                                toBreak = 1;
                              });
                }
                else
                {
                  // evict the client from the file-system
                  if (evictClient(idWithLock->id, fs, pipe) == -1)
                  {
                    perror("error during the eviction of a client");
                  }

                  // close connection
                  int minusOne = -1;
                  HANDLE_WRNS(writen(pipe, &minusOne, sizeof(minusOne)), sizeof(minusOne), NOOP,
                              {
                                perror("failed communication with main thread");
                              });

                  CLOSE(idWithLock->id, "cannot close the communication with a client");
                }

                free(idWithLock);
              }
            }
            free(pathname);

            if (!closeConnection)
            {
              sendBackFileDescriptor = 1;
            }
          }
          break;
        }

        default:
        {
          closeConnection = 1;
          break;
        }
        }
      }

      if (closeConnection)
      {
        // remove the client from the file-system
        int ec = evictClient(fd, fs, pipe);

        if (ec == -1)
        {
          perror("error during the eviction of a client");
        }

        int minusOne = -1;
        HANDLE_WRNS(writen(pipe, &minusOne, sizeof(minusOne)), sizeof(minusOne), NOOP,
                    {
                      perror("failed communication with main thread");
                    });

        CLOSE(fd, "cannot close the communication with a client");
      }

      if (sendBackFileDescriptor)
      {
        // sent the fd back
        HANDLE_WRNS(writen(pipe, &fd, sizeof(fd)), sizeof(fd), NOOP,
                    {
                      perror("failed communication with main thread");
                      toBreak = 1;
                    });
      }
    }
  }

  return NULL;
}

int main(int argc, char **argv)
{
  int error = 0;

  // ----------------------------------------------------------------------

  // create pipes

  // on 0 the main thread reads which signal has arrived
  // on 1 the signal handler write such signals
  int masterSigHandlerPipe[2];
  AAINZ(pipe(masterSigHandlerPipe), "masterSigHandlerPipe initialization has failed")

  // on 0 the main thread reads which fds are ready to be setted into the fd_set set
  // on 1 workers write such fds
  int masterWorkersPipe[2];
  AAINZ(pipe(masterWorkersPipe), "masterWorkersPipe initialization has failed")

  // ----------------------------------------------------------------------

  // run signal handler
  createDetachSigHandlerThread(masterSigHandlerPipe[1]);

  maskSIGPIPEAndHandledSignals();

  // ----------------------------------------------------------------------

  char *configPath = argv[1];
  AAIN(configPath, "missing configuration path");

  // parser the configuration file
  ConfigParser parser = ConfigParser_parse(configPath, &error);
  if (error)
  {
    puts(ConfigParser_getErrorMessage(error));
    exit(EXIT_FAILURE);
  }

  // ----------------------------------------------------------------------
  // read the configurations file

  // - socket file name
  char *SOCK_NAME = ConfigParser_getValue(parser, "SOCKNAME", &error);
  if (error)
  {
    puts(ConfigParser_getErrorMessage(error));
    exit(EXIT_FAILURE);
  }
  AAIN(SOCK_NAME, "invalid SOCKNAME setting");
  SOCKNAME = strdup(SOCK_NAME);
  AAIN(SOCKNAME, "internal strdup error");

  // - number of workers
  char *_N_OF_WORKERS = ConfigParser_getValue(parser, "N_OF_WORKERS", &error);
  int N_OF_WORKERS = 0;
  if (error)
  {
    puts(ConfigParser_getErrorMessage(error));
    exit(EXIT_FAILURE);
  }
  else
  {
    AAIN(_N_OF_WORKERS, "invalid N_OF_WORKERS setting");
    N_OF_WORKERS = atoi(_N_OF_WORKERS);
    if (N_OF_WORKERS <= 0)
    {
      puts("invalid N_OF_WORKERS setting");
      exit(EXIT_FAILURE);
    }
  }

  // - logger file
  char *LOG_FILE = ConfigParser_getValue(parser, "LOG_FILE", &error);
  if (error)
  {
    puts(ConfigParser_getErrorMessage(error));
    exit(EXIT_FAILURE);
  }
  AAIN(LOG_FILE, "invalid LOG_FILE setting");

  // - file-system settings
  char *_MAX_FS_SIZE = ConfigParser_getValue(parser, "MAX_FS_SIZE", &error);
  int MAX_FS_SIZE = 0;
  if (error)
  {
    puts(ConfigParser_getErrorMessage(error));
    exit(EXIT_FAILURE);
  }
  else
  {
    AAIN(_MAX_FS_SIZE, "invalid MAX_FS_SIZE setting");
    MAX_FS_SIZE = atoi(_MAX_FS_SIZE);
    if (MAX_FS_SIZE <= 0)
    {
      puts("invalid MAX_FS_SIZE setting");
      exit(EXIT_FAILURE);
    }
  }
  char *_MAX_FS_N_FILES = ConfigParser_getValue(parser, "MAX_FS_N_FILES", &error);
  int MAX_FS_N_FILES = 0;
  if (error)
  {
    puts(ConfigParser_getErrorMessage(error));
    exit(EXIT_FAILURE);
  }
  else
  {
    AAIN(_MAX_FS_N_FILES, "invalid MAX_FS_N_FILES setting");
    MAX_FS_N_FILES = atoi(_MAX_FS_N_FILES);
    if (MAX_FS_N_FILES <= 0)
    {
      puts("invalid MAX_FS_N_FILES setting");
      exit(EXIT_FAILURE);
    }
  }
  char *_EVICTION_POLICY = ConfigParser_getValue(parser, "EVICTION_POLICY", &error);
  int EVICTION_POLICY = 0;
  if (error)
  {
    puts(ConfigParser_getErrorMessage(error));
    exit(EXIT_FAILURE);
  }
  else
  {
    AAIN(_EVICTION_POLICY, "invalid EVICTION_POLICY setting");
    if (strncmp(_EVICTION_POLICY, "FIFO", strlen("FIFO")))
    {
      EVICTION_POLICY = FS_REPLACEMENT_FIFO;
    }
    else if (strncmp(_EVICTION_POLICY, "LRU", strlen("LRU")))
    {
      EVICTION_POLICY = FS_REPLACEMENT_LRU;
    }
    else
    {
      puts("invalid EVICTION_POLICY setting");
      exit(EXIT_FAILURE);
    }
  }

  // ----------------------------------------------------------------------

  // set up the file-system

  FileSystem fs = FileSystem_create(MAX_FS_SIZE, MAX_FS_N_FILES, EVICTION_POLICY, &error);

  if (error)
  {
    puts(FileSystem_getErrorMessage(error));
    puts("cannot create the file-system");
    exit(EXIT_FAILURE);
  }
  // ----------------------------------------------------------------------

  // set up the logger
  Logger_create(LOG_FILE, &error);
  if (error)
  {
    puts(Logger_getErrorMessage(error));
    puts("cannot create the logger");
    exit(EXIT_FAILURE);
  }

  // ----------------------------------------------------------------------

  // set cleanup function to remove the socket file
  AAINZ(atexit(end), "set of the cleanup function has failed")
  // ----------------------------------------------------------------------

  // thread safe queue to send to the workers the ready fds
  // of the clients
  SimpleQueue sq = SimpleQueue_create(&error);
  HANDLE_QUEUE_ERROR_ABORT(error);

  // ----------------------------------------------------------------------

  // create server socket

  int fd_skt = setupServerSocket(SOCKNAME, UNIX_PATH_MAX - 1);

  // ----------------------------------------------------------------------

  // launch workers
  WorkerContext ctx;
  ctx.pipe = masterWorkersPipe[1];
  ctx.sq = sq;
  ctx.fs = fs;
  pthread_t workers[N_OF_WORKERS];
  for (int i = 0; i < N_OF_WORKERS; i++)
  {
    AAINZ(pthread_create(workers + i, NULL, worker, &ctx), "creation of a worker thread has failed")
  }
  char toLog[LOG_LEN] = {0};
  sprintf(toLog, "%d workers launched -", N_OF_WORKERS);
  LOG(toLog)

  // ----------------------------------------------------------------------

  // select machinery

  int fd_num = 0,
      fd;
  fd_set set, rdset;

  // determine the higher fd for now
  if (fd_skt > fd_num)
  {
    fd_num = fd_skt;
  }
  if (masterWorkersPipe[0] > fd_num)
  {
    fd_num = masterWorkersPipe[0];
  }
  if (masterSigHandlerPipe[0] > fd_num)
  {
    fd_num = masterSigHandlerPipe[0];
  }

  FD_ZERO(&set);
  FD_SET(fd_skt, &set);
  FD_SET(masterWorkersPipe[0], &set);
  FD_SET(masterSigHandlerPipe[0], &set);

  // will be filled when a signal will come
  int signal = 0;

  // number of connected clients
  int nOfConnectedClients = 0;

  // stop if a signal has been handled
  while (signal != RAGE_QUIT && (signal != SOFT_QUIT || nOfConnectedClients > 0))
  {

    rdset = set;
    int sel_res = select(fd_num + 1, &rdset, NULL, NULL, NULL);
    if (sel_res == -1 && errno == EINTR)
    {
      perror("select function was interrupted by a signal");
      exit(EXIT_FAILURE);
    }
    else if (sel_res == -1)
    {
      perror("select function has raised an error");
      exit(EXIT_FAILURE);
    }
    else
    {
      for (fd = 0; fd <= fd_num && signal != RAGE_QUIT && (signal != SOFT_QUIT || nOfConnectedClients > 0); fd++)
      {
        if (FD_ISSET(fd, &rdset))
        {
          // refuse new connection when a signal has arrived
          if (fd == fd_skt && signal == 0)
          {
            // accept handling
            int fd_c = accept(fd_skt, NULL, 0);

            if (fd_c == -1 && errno == EINTR)
            {
              perror("accept function was interrupted by a signal");
              exit(EXIT_FAILURE);
            }
            else if (fd_c == -1)
            {
              perror("accept function has raised an error");
              exit(EXIT_FAILURE);
            }

            // increase the number of connected clients
            nOfConnectedClients++;

            char toLog[LOG_LEN] = {0};
            sprintf(toLog, "New connection, %d clients currently connected -", nOfConnectedClients);
            LOG(toLog)

            // connection handling
            FD_SET(fd_c, &set);
            if (fd_c > fd_num)
            {
              fd_num = fd_c;
            }
          }
          else if (fd == masterWorkersPipe[0]) // a worker has done its job regarding one particular client
          {
            // retrieve the client of which request was handled
            // or -1 if a client has disconnected
            int fd;

            HANDLE_WRNS(readn(masterWorkersPipe[0], &fd, sizeof(fd)), sizeof(fd), ;, error = 1;);

            if (error)
            {
              perror("something bad has appened trying to read from masterWorkersPipe");
              exit(EXIT_FAILURE);
            }
            else
            {
              if (fd == -1)
              {
                // decrease the number of connected clients
                nOfConnectedClients--;
                char toLog[LOG_LEN] = {0};
                sprintf(toLog, "A client has disconnected, %d clients currently connected -", nOfConnectedClients);
                LOG(toLog)
              }
              else
              {
                FD_SET(fd, &set);
                if (fd > fd_num)
                {
                  fd_num = fd;
                }
              }
            }
          }
          else if (fd == masterSigHandlerPipe[0]) // a signal has arrived
          {

            // retrieve the signal
            HANDLE_WRNS(readn(masterSigHandlerPipe[0], &signal, sizeof(signal));, sizeof(signal), ;, error = 1;);

            if (error)
            {
              perror("something bad has appened trying to read from masterSigHandlerPipe");
              exit(EXIT_FAILURE);
            }
          }
          else
          { // if an already known client (fd) has a new request, send it to the workers
            char toLog[LOG_LEN] = {0};
            sprintf(toLog, "Request received from client (fd) %d -", fd);
            LOG(toLog)

            // remove the fd from the set
            FD_CLR(fd, &set);
            if (fd == fd_num)
            {
              fd_num--;
            }

            // control flow flags
            int shouldClose = 0;
            int shouldExit = 0;

            int *fdp = malloc(sizeof(*fdp));
            if (fdp == NULL)
            {
              shouldClose = 1;
            }
            else
            {
              // send the file descriptor to the workers
              *fdp = fd;
              SimpleQueue_enqueue(sq, fdp, &error);

              if (error == E_SQ_MUTEX_COND || error == E_SQ_MUTEX_LOCK)
              {
                perror("Mutex error");
                shouldClose = 1;
                shouldExit = 1;
              }
              else if (error)
              {
                shouldClose = 1;
              }
            }

            if (shouldClose)
            {
              close(fd);
            }

            if (shouldExit)
            {
              exit(EXIT_FAILURE);
            }
          }
        }
      }
    } // close while(1)
  }

  LOG("Shutdown process initiated")

  // ----------------------------------------------------------------------

  // delete the queue, signaling to the workers
  // that they must return
  SimpleQueue_delete(&sq, &error);
  if (error)
  {
    puts(SimpleQueue_getErrorMessage(error));
    error = 0;
  }
  else
  {
    LOG("Thread-safe queue correctly freed")
  }

  // ----------------------------------------------------------------------

  for (int i = 0; i < N_OF_WORKERS; i++)
  {
    errno = 0;
    pthread_join(workers[i], NULL);
    if (errno)
    {
      perror("something has gone wrong in pthread_join - skipping join process");
      LOG("Something has gone wrong in pthread_join - skipping join process")
      break;
    }
  }
  if (!errno)
  {
    LOG("Threads correctly joined")
  }

  // ----------------------------------------------------------------------

  // free the parser
  ConfigParser_delete(&parser, &error);
  if (error)
  {
    puts(ConfigParser_getErrorMessage(error));
    error = 0;
  }
  else
  {
    LOG("Config parsers correctly freed")
  }

  // ----------------------------------------------------------------------

  // free the file-system
  FileSystem_delete(&fs, &error);
  if (error)
  {
    puts(FileSystem_getErrorMessage(error));
    error = 0;
  }
  else
  {
    LOG("File-system correctly freed")
  }

  // ----------------------------------------------------------------------

  // close the server socket fd
  int close_res;
  while (((close_res = close(fd_skt)) == -1) && (errno == EINTR))
    ;
  if (close_res == -1)
  {
    perror("closing the server socket fd has failed");
    LOG("closing the server socket fd has failed")
    return -1;
  }
  else
  {
    LOG("Connection correctly closed")
  }

  // ----------------------------------------------------------------------

  // free the logger
  Logger_delete(1, &error);
  if (error)
  {
    puts(Logger_getErrorMessage(error));
    error = 0;
  }

  // ----------------------------------------------------------------------

  return 0;
}