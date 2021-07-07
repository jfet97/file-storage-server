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
#include "simple_queue.h"
#include "config-parser.h"
#include "logger.h"

#define NOOP ;

#define UNIX_PATH_MAX 108 // TODO: ???

#define BUF _POSIX_PIPE_BUF

#define SOFT_QUIT 1
#define RAGE_QUIT 2

#define LOG_LEN 128

// ABORT_ABRUPTLY_IF_NON_ZERO
#define AAINZ(code, message) \
  if (code != 0)             \
  {                          \
    perror(message);         \
    exit(EXIT_FAILURE);      \
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

#define HANDLE_WRN(A, S, OK, NE, IZ, IE) \
  errno = 0;                             \
  int r = A;                             \
  if (r == S)                            \
  {                                      \
    OK                                   \
  }                                      \
  else if (r == 0)                       \
  {                                      \
    IZ                                   \
  }                                      \
  else if (r == -1)                      \
  {                                      \
    IE                                   \
  }                                      \
  else if (r < S)                        \
  {                                      \
    NE                                   \
  }

#define HANDLE_WRNS(A, S, OK, KO) \
  HANDLE_WRN(A, S, OK, KO, KO, KO)

#define CLOSE(FD, S) \
  errno = 0;         \
  int c = close(FD); \
  if (c == -1)       \
  {                  \
    perror(S);       \
  }

#define LOG(M)                         \
  {                                    \
    int e = 0;                         \
    Logger_log(M, strlen(M), &e);      \
    if (e)                             \
    {                                  \
      puts(Logger_getErrorMessage(e)); \
    }                                  \
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

typedef struct
{
  SimpleQueue sq;
  int pipe;
} WorkerContext;

// signal handler callback
static void sig_handler_cb(int signum, int pipe)
{
  int toSend = 0;

  char toLog[LOG_LEN] = {0};
  sprintf(toLog, "Received signal %d -", signum);
  LOG(toLog);
  FLUSH_LOGGER;

  if (signum == SIGTSTP || signum == SIGTERM)
  {
    printf("\nshutting down soon because of %d...\n", signum);
    exit(EXIT_SUCCESS);
  }
  else if (signum == SIGQUIT || signum == SIGINT)
  {
    toSend = SOFT_QUIT;
  }
  else if (signum == SIGHUP)
  {
    toSend = RAGE_QUIT;
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
void *sig_handler(void *arg)
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

// cleanup function
void end(int exit, void *arg)
{
  char *sockname = arg;
  if (sockname)
  {
    // remove the socket file
    remove(sockname);
  }
  printf("Exited with code: %d\n", exit);
  puts("Goodbye, cruel world....");
}

// create and detach the signal handler thread
void createDetachSigHandlerThread(int pipe)
{
  pthread_t sig_handler_thread;
  int *pipePtr = malloc(sizeof(*pipePtr));
  AAIN(pipePtr, "the creation of the signal handler thread has failed")
  *pipePtr = pipe;
  AAINZ(pthread_create(&sig_handler_thread, NULL, sig_handler, pipePtr), "the creation of the signal handler thread has failed")
  AAINZ(pthread_detach(sig_handler_thread), "the detachment of the signal handler thread has failed")
}

// mask the SIGPIPE signal
void maskSIGPIPEAndHandledSignals()
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

int setupServerSocket(char *sockname, int upm)
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
    exit(EXIT_SUCCESS);
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
    exit(EXIT_SUCCESS);
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
    exit(EXIT_SUCCESS);
  }
  else if (el == -1)
  {
    perror("listen function has raised an error");
    exit(EXIT_FAILURE);
  }

  return fd_skt;
}

void *worker(void *args)
{
  WorkerContext *ctx = args;
  SimpleQueue sq = ctx->sq;
  int pipe = ctx->pipe;

  int error = 0;
  int toBreak = 0;

  while (!toBreak)
  { // TODO: stop se flag
    // get a file descriptor
    int *fdPtr = SimpleQueue_dequeue(sq, 1, &error);
    if (error)
    {
      puts(SimpleQueue_getErrorMessage(error));
      toBreak = 1;
    }
    else
    {
      int fd = *fdPtr;
      free(fdPtr);

      // read a message
      char buf[1000] = {0};
      HANDLE_WRN(
          readn(fd, buf, 9),
          9,
          {
            // if it is all ok

            // TODO: da sradicare
            // puts(buf);
            writen(fd, "CIAO", strlen("CIAO"));

            // sent the fd back
            HANDLE_WRNS(writen(pipe, &fd, sizeof(fd)), sizeof(fd), NOOP,
                        {
                          perror("failed communication with main thread");
                          toBreak = 1;
                        });
          },
          { // if not enough characters were read, close the connection
            CLOSE(fd, "cannot close the communication with a client");
          },
          { // the client has closed the connection, free the resources related to it
            // signal the eevent to the main thread
            int minusOne = -1;
            HANDLE_WRNS(writen(pipe, &minusOne, sizeof(minusOne)), sizeof(minusOne), NOOP,
                        {
                          perror("failed communication with main thread");
                        });
            CLOSE(fd, "cannot close the communication with a client");
          },
          {
            // if an error has occurred, try to close the connection
            perror("Cannot receive data from a client, the connection will be closed");
            CLOSE(fd, "cannot close the communication with a client");
          })
    }
  }

  return NULL;
}

int main(int argc, char **argv)
{
  int error = 0;

  char *configPath = argv[1];
  AAIN(configPath, "missing configuration path");

  // parser that reads the configuration file
  ConfigParser parser = ConfigParser_parse(configPath, &error);
  if (error)
  {
    puts(ConfigParser_getErrorMessage(error));
    exit(EXIT_FAILURE);
  }

  // read the needed configurations
  // socket file name
  char *SOCKNAME = ConfigParser_getValue(parser, "SOCKNAME", &error);
  if (error)
  {
    puts(ConfigParser_getErrorMessage(error));
    exit(EXIT_FAILURE);
  }
  AAIN(SOCKNAME, "invalid SOCKNAME setting");

  // number of workers
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

  // logger file
  char *LOG_FILE = ConfigParser_getValue(parser, "LOG_FILE", &error);
  if (error)
  {
    puts(ConfigParser_getErrorMessage(error));
    exit(EXIT_FAILURE);
  }
  AAIN(LOG_FILE, "invalid LOG_FILE setting");

  // set up the logger
  Logger_create(LOG_FILE, &error);
  if (error)
  {
    puts(Logger_getErrorMessage(error));
    puts("cannot create the logger");
    exit(EXIT_FAILURE);
  }

  // set cleanup function to remove the socket file
  AAINZ(on_exit(end, SOCKNAME), "set of the cleanup function has failed")

  // thread safe queue to send to the workers the ready fds
  // of the clients
  SimpleQueue sq = SimpleQueue_create(&error);
  HANDLE_QUEUE_ERROR_ABORT(error);

  // on 0 the main thread reads which signal has arrived
  // on 1 the signal handler write such signals
  int masterSigHandlerPipe[2];
  AAINZ(pipe(masterSigHandlerPipe), "masterSigHandlerPipe initialization has failed")

  // on 0 the main thread reads which fds are ready to be setted into the fd_set set
  // on 1 workers write such fds
  int masterWorkersPipe[2];
  AAINZ(pipe(masterWorkersPipe), "masterWorkersPipe initialization has failed")

  createDetachSigHandlerThread(masterSigHandlerPipe[1]);

  maskSIGPIPEAndHandledSignals();

  int fd_skt = setupServerSocket(SOCKNAME, UNIX_PATH_MAX);

  // workers
  WorkerContext ctx;
  ctx.pipe = masterWorkersPipe[1];
  ctx.sq = sq;
  pthread_t workers[N_OF_WORKERS];
  for (int i = 0; i < N_OF_WORKERS; i++)
  {
    AAINZ(pthread_create(workers + i, NULL, worker, &ctx), "creation of a worker thread has failed")
  }
  char toLog[LOG_LEN] = {0};
  sprintf(toLog, "%d workers launched -", N_OF_WORKERS);
  LOG(toLog)

  //---------------------------------------------------------

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
  while (signal != RAGE_QUIT && (signal != SOFT_QUIT || nOfConnectedClients > 0)) // TODO: gestire a modo i due segnali diversi, evitare timeout...usare pipe apposita
  {

    rdset = set;
    int sel_res = select(fd_num + 1, &rdset, NULL, NULL, NULL);
    if (sel_res == -1 && errno == EINTR)
    {
      perror("select function was interrupted by a signal");
      exit(EXIT_SUCCESS);
    }
    else if (sel_res == -1)
    {
      perror("select function has raised an error");
      exit(EXIT_FAILURE);
    }
    else if (sel_res == 0)
    {
      // timeout: go to check if a signal has arrrived
      continue;
    }
    else
    {
      for (fd = 0; fd <= fd_num && signal != RAGE_QUIT && (signal != SOFT_QUIT || nOfConnectedClients > 0); fd++)
      {
        if (FD_ISSET(fd, &rdset))
        {
          // refuse new connection when a signal has arrived
          if (fd == fd_skt && signal == 0) // new connection
          {
            // accept handling
            int fd_c = accept(fd_skt, NULL, 0);

            if (fd_c == -1 && errno == EINTR)
            {
              perror("accept function was interrupted by a signal");
              exit(EXIT_SUCCESS);
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

            readn(masterWorkersPipe[0], &fd, sizeof(fd)); // TODO: gestire a modo errori

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
          else if (fd == masterSigHandlerPipe[0]) // a signal has arrived
          {

            // retrieve the signal
            readn(masterSigHandlerPipe[0], &signal, sizeof(signal)); // TODO: gestire a modo errori
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
    } /* chiude while(1) */
  }

  if (signal)
  {
    // delete the queue, signaling to the workers
    // that they must return
    SimpleQueue_delete(&sq, &error);
    if (error)
    {
      puts(SimpleQueue_getErrorMessage(error));
      error = 0;
    }
  }

  // TODO: check errori
  for (int i = 0; i < N_OF_WORKERS; i++)
  {
    pthread_join(workers[i], NULL);
  }

  // free the parser
  ConfigParser_delete(&parser, &error);
  if (error)
  {
    puts(ConfigParser_getErrorMessage(error));
    error = 0;
  }

  // free the logger
  Logger_delete(1, &error);
  if (error)
  {
    puts(Logger_getErrorMessage(error));
    error = 0;
  }

  // close the server socket fd
  int close_res;
  while (((close_res = close(fd_skt)) == -1) && (errno == EINTR))
    ;
  if (close_res == -1)
  {
    perror("closing the socket socket fd has failed");
    return -1;
  }

  return 0;
}

// TODO: join dei thread