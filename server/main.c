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

#define UNIX_PATH_MAX 108   // TODO: ???
#define SOCKNAME "./myssss" // TODO: lo deve leggere dal file di configurazione

#define BUF _POSIX_PIPE_BUF // TODO: ???

#define ec(s, r, m)     \
  if ((s) == (r))       \
  {                     \
    perror(m);          \
    exit(EXIT_FAILURE); \
  }

#define ec_n(s, r, m)   \
  if ((s) != (r))       \
  {                     \
    perror(m);          \
    exit(EXIT_FAILURE); \
  }

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

// ssize_t /* Read "n" bytes from a descriptor */
// readn(int fd, void *ptr, size_t n)
// {
//     size_t nleft;
//     ssize_t nread;

//     nleft = n;
//     while (nleft > 0)
//     {
//         if ((nread = read(fd, ptr, nleft)) < 0)
//         {
//             if (nleft == n)
//                 return -1; /* error, return -1 */
//             else
//                 break; /* error, return amount read so far */
//         }
//         else if (nread == 0) {
//             break; /* EOF */
//         }
//         nleft -= nread;
//         ptr += nread;
//     }
//     return (n - nleft); /* return >= 0 */
// }

// ssize_t /* Write "n" bytes to a descriptor */
// writen(int fd, void *ptr, size_t n)
// {
//     size_t nleft;
//     ssize_t nwritten;

//     nleft = n;
//     while (nleft > 0)
//     {
//         if ((nwritten = write(fd, ptr, nleft)) < 0)
//         {
//             if (nleft == n)
//                 return -1; /* error, return -1 */
//             else
//                 break; /* error, return amount written so far */
//         }
//         else if (nwritten == 0)
//             break;
//         nleft -= nwritten;
//         ptr += nwritten;
//     }
//     return (n - nleft); /* return >= 0 */
// }

// used to signal the arrival of a signal
volatile sig_atomic_t sig_flag = 0;

// signal handler callback
static void sig_handler_cb(int signum)
{
  printf("\nshutting down soon because of %d...\n", signum);
  sig_flag = 1;
}

// signal handler (dedicated thread)
void *sig_handler(void *arg)
{
  sigset_t sett;

  // initialize the POSIX signal set
  AAINZ(sigemptyset(&sett), "set of the POSIX signal set has failed")

  // add to the POSIX set the signals of interest
  AAINZ(sigaddset(&sett, SIGINT), "manipulation of POSIX signal set has failed")
  AAINZ(sigaddset(&sett, SIGQUIT), "manipulation of POSIX signal set has failed")
  AAINZ(sigaddset(&sett, SIGTSTP), "manipulation of POSIX signal set has failed")
  AAINZ(sigaddset(&sett, SIGTERM), "manipulation of POSIX signal set has failed")
  AAINZ(sigaddset(&sett, SIGHUP), "manipulation of POSIX signal set has failed")

  // apply the mask
  AAINZ(pthread_sigmask(SIG_SETMASK, &sett, NULL), "change mask of blocked signals has failed")

  int signum;

  // wait a signal
  AAINZ(sigwait(&sett, &signum), "suspension of execution of the signal handler thread has failed")

  sig_handler_cb(signum);

  return NULL;
}

// cleanup function
void end(int exit, void *arg)
{
  char *sockname = arg;
  if (sockname)
  {
    // remove the socket file
    remove(SOCKNAME);
  }
  printf("Exited with code: %d\n", exit);
  puts("Goodbye, cruel world....");
}

// create and detach the signal handler thread
void createDetachSigHandlerThread()
{
  pthread_t sig_handler_thread;
  AAINZ(pthread_create(&sig_handler_thread, NULL, sig_handler, NULL), "the creation of the signal handler thread has failed")
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

int main(int argc, char **argv)
{
  // set cleanup function to remove the socket file
  AAINZ(atexit(end), "set of the cleanup function has failed")

  createDetachSigHandlerThread();

  maskSIGPIPEAndHandledSignals();

  int fd_skt = setupServerSocket(SOCKNAME, UNIX_PATH_MAX);

  //---------------------------------------------------------

  // select machinery

  int fd_num = 0, fd;
  fd_set set, rdset;
  if (fd_skt > fd_num)
  {
    fd_num = fd_skt;
  }
  FD_ZERO(&set);
  FD_SET(fd_skt, &set);

  // stop if a signal has been handled
  while (!sig_flag) // TODO: gestire a modo i due segnali diversi
  {
    // timeout to periodically wake up the select to check the sig_flag flag
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000; // 0.5 seconds

    rdset = set;
    int sel_res = select(fd_num + 1, &rdset, NULL, NULL, &timeout);
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
      for (fd = 0; fd <= fd_num && !sig_flag; fd++)
      {
        if (FD_ISSET(fd, &rdset))
        {
          if (fd == fd_skt)
          {
            // accept a new connection
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

            FD_SET(fd_c, &set);
            if (fd_c > fd_num)
            {
              fd_num = fd_c;
            }
            break;
          }
          else
          {
            // handle the request

            char *buf = malloc(_POSIX_PIPE_BUF * sizeof(*buf));
            AAIN(buf, "malloc has failed");

            if (sig_flag == 1)
            {
            }

            // leggo la stringa contenente l'espressione da trasformare
            int re = read(fd, buf, BUF);
            if (re == -1)
            {
              perror("READ FALLITA MALE");
              free(buf);
              break;
            }

            // struct pollfd * pfds = calloc(1, sizeof(struct pollfd));
            // pfds[0].fd = fd;
            // pfds[0].events = POLLIN;

            // int poll_res;

            // while((poll_res = poll(pfds,  1,  2000)) == 0) {
            //     if(sig_flag == 1) {
            //         break;
            //     }
            // }

            // if(poll_res == -1) {
            //     perror("poll SFANCULATA");
            //     free(buf);
            //     close(fd);
            //     break;
            // }

            if (re == 0)
            {
              FD_CLR(fd, &set);
              if (fd == fd_num)
                fd_num--;

              int close_res;
              while (((close_res = close(fd)) == -1) && (errno == EINTR))
                ;
              if (close_res == -1)
              {
                perror("CLOSE FALLITA MALE");
                break;
              }
            }
            else
            {
              char *res = malloc(_POSIX_PIPE_BUF * sizeof(*res));

              strcpy(res, buf);
              char *r = res;
              while (*r)
              {
                *r = toupper(*r);
                r++;
              }

              // lo spedisco al client
              int wres = write(fd, res, strlen(res) + 1);
              if (wres == -1)
              {
                perror("problemi con writen");
                free(res);
                break;
              }

              free(res);
            }

            free(buf);
          }
        }
      }

    } /* chiude while(1) */
  }

  int close_res;
  while (((close_res = close(fd_skt)) == -1) && (errno == EINTR))
    ;
  if (close_res == -1)
  {
    perror("CLOSE FALLITA MALE");
    return -1;
  }

  return 0;
}