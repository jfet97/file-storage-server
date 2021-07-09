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
#include <sys/socket.h>
#include <sys/un.h>
#include "api.h"
#include "command-line-parser.h"
#include "list.h"

#define O_CREATE 1
#define O_LOCK 2

// ABORT_ABRUPTLY_IF_NULL
#define AAIN(code, message, action) \
  if (code == NULL)                 \
  {                                 \
    perror(message);                \
    action;                         \
  }

// ABORT_ABRUPTLY_IF_NEG_ONE
#define AAINO(code, message, action) \
  if (code == -1)                    \
  {                                  \
    perror(message);                 \
    action;                          \
  }
// ABORT_ABRUPTLY_IF_NON_ZERO
#define AAINZ(code, message, action) \
  if (code != 0)                     \
  {                                  \
    perror(message);                 \
    action;                          \
  }

static int
isCurrentDir(const char *name)
{
  if (strcmp(name, ".") == 0)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

static int isParentDir(const char *name)
{
  if (strcmp(name, "..") == 0)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

// used to write a list of files
static void writeOptionCallback(void *rawDirname, void *rawFilePath, int *error)
{

  if (*error)
  {
    return;
  }

  // convert nack to NULL if the rawDirname is an empty string
  char *dirname = strcmp(rawDirname, "") == 0 ? NULL : rawDirname;
  char *file = rawFilePath;

  errno = 0;
  // try to open the file with both flags
  openFile(file, O_CREATE | O_LOCK);

  // if the operation has ended successfully
  if (errno == 0)
  {
    writeFile(file, dirname);
    // is a bad bad error only if it is different from EPERM
    if (errno != EPERM)
    {
      *error = errno;
    }
  }
  else
  {
    *error = errno;
  }
}

// read *nPtr files recursively from a directory
static int
readNFilesFromDir(const char *dirname, int *nPtr, List_T readFiles)
{

  AAIN(readFiles, "readFiles is NULL in readNFilesFromDir", return -1;)
  AAIN(nPtr, "nPtr is NULL in readNFilesFromDir", return -1;)

  int readAllFiles = *nPtr <= 0;

  // open the dir
  DIR *dir = opendir(dirname);
  AAIN(dir, "opendir has failed in readNFilesFromDir", return -1;)

  // cd into the dir
  AAINZ(chdir(dirname), "chdir has failed in readNFilesFromDir", AAINZ(closedir(dir), "closedir has failed in readNFilesFromDir", return -1;))

  struct dirent *file = NULL;
  // read each entry of the directory, recursively, until n/all files have been read
  while ((readAllFiles || *nPtr) && (file = readdir(dir)) != NULL)
  {
    char *filename = file->d_name;
    struct stat s;
    stat(filename, &s);
    AAINZ(stat(filename, &s), "stat has failed in readNFilesFromDir", AAINZ(closedir(dir), "closedir has failed in readNFilesFromDir", return -1;))

    int isFileCurrentDir = isCurrentDir(filename);
    int isFileParentDir = isParentDir(filename);
    int isDir = S_ISDIR(s.st_mode);
    int isRegFile = S_ISREG(s.st_mode);

    // if it is a special file, skip it
    if (isFileCurrentDir || isFileParentDir || (!isDir && !isRegFile))
    {
      continue;
    }
    else if (isDir)
    {
      int recRes = readNFilesFromDir(filename, nPtr, readFiles);

      if (recRes == -1)
      {
        AAINZ(closedir(dir), "closedir has failed in readNFilesFromDir", return -1;)
      }
      else
      {
        // cd in the current dir again
        AAINZ(chdir(".."), "chdir has failed in readNFilesFromDir", AAINZ(closedir(dir), "closedir has failed in readNFilesFromDir", return -1;))
      }
    }
    else if (isRegFile)
    {
      char *path = absolutify(filename);
      AAIN(path, "internal error in readNFilesFromDir", AAINZ(closedir(dir), "closedir has failed in readNFilesFromDir", return -1;))
      int error = 0;
      List_insertHead(readFiles, path, &error);
      if (error)
      {
        puts(List_getErrorMessage(error));
        free(path);
        AAINZ(closedir(dir), "closedir has failed in readNFilesFromDir", return -1;)
      }
      else
      {
        if (!readAllFiles)
        {
          (*nPtr)--;
        }
      }
    }
  }

  AAINZ(closedir(dir), "closedir has failed in readNFilesFromDir", return -1;)

  return 0;
}

int main(int argc, char **argv)
{

  // control flow flags
  int error = 0;
  int stop = 0;

  const char *socket = "";              // socket file used for communication with the server
  int timeToWaitBetweenConnections = 0; // set using option t

  // setup the command line arguments parser
  Arguments as = CommandLineParser_parseArguments(argv + 1, &error);
  if (error)
  {
    perror(CommandLineParser_getErrorMessage(error));
    return -1;
  }

  // obtain a simple linked list of the arguments
  Option *asList = CommandLineParser_getArguments(as, &error);
  if (error)
  {
    perror(CommandLineParser_getErrorMessage(error));
    return -1;
  }

  // search for option h
  for (Option *p = asList; p && !stop; p = p->next)
  {
    char op = p->op;
    if (op == 'h')
    {
      stop = 1;
      printf("\nUsage: %s -h\n"
             "Usage: %s -f socketPath [-w dirname [n=0]] [-Dd dirname] [-p]"
             "[-t [0]] [-R [n=0]] [-Wrluc file1 [,file2,file3 ...]]\n"
             "Options:\n"
             "  -h,\tPrints the info message\n"
             "  -O,\tSets the dir for openFile eviction. If no dir is setted, the file/s will be lost"
             "  -f,\tSets the socket file path up to the specified socketPath\n"
             "  -w,\tSends to the server n files from the specified dirname directory. If n=0, sends every file in dirname\n"
             "  -W,\tSends to the server the specified files\n"
             "  -D,\tIf capacity misses occur on the server, save the files it gets to us in the specified dirname directory\n"
             "  -r,\tReads the specified files from the server\n"
             "  -R,\tReads n random files from the server (if n=0, reads every file)\n"
             "  -d,\tSaves files read with -r or -R option in the specified dirname directory\n"
             "  -t,\tSpecifies the time between two consecutive requests to the server\n"
             "  -l,\tLocks the specified files\n"
             "  -u,\tUnlocks the specified files\n"
             "  -c,\tRemoves the specified files from the server\n"
             "  -p,\tPrints information about operation performed on the server\n",
             argv[0], argv[0]);
    }
  }

  // check if option f is the first option and check if options t and p are used only one time
  int tTimes = 0;
  int fTimes = 0;
  int pTimes = 0;
  int fFirst = 0;
  for (Option *p = asList; p && !stop; p = p->next)
  {
    char op = p->op;
    if (op == 't')
    {
      // save the timeToWaitBetweenConnections time
      timeToWaitBetweenConnections = atoi(p->param);
      tTimes++;
    }
    if (op == 'f')
    {
      fFirst = p == asList;
      fTimes++;
    }
    if (op == 'p')
    {
      pTimes++;
    }
  }
  if (!fFirst)
  {
    puts("Error: option f has to be the first used option");
    error = 1;
  }
  if (fTimes != 1)
  {
    puts("Error: option f has to be used one and only one time");
    error = 1;
  }
  if (pTimes > 1)
  {
    puts("Error: option p has to be used at most one time");
    error = 1;
  }
  if (tTimes > 1)
  {
    puts("Error: option t has to be used at most one time");
    error = 1;
  }
  if (error)
  {
    stop = 1;
  }

  // do only if no h option was passed
  for (Option *p = asList; p && !stop; p = p->next)
  {
    char op = p->op;
    const char *param = p->param;

    switch (op)
    {
    case 'f':
    {
      // save the socket file's name and open the connection
      socket = param;

      struct timespec abstime;
      time_t now = time(NULL);
      AAINO(now, "internal time error", stop = 1; error = 1; break;)

      // try for two minutes to connect to the client
      abstime.tv_sec = now + 120;
      openConnection(socket, timeToWaitBetweenConnections ? timeToWaitBetweenConnections : 100, abstime);
      break;
    }
    case 'w':
    {

      // clone the param string
      char *paramClone = strdup(param);
      AAIN(paramClone, "strdup has failed during the handling of option w", stop = 1; error = 1; break;)

      char *dirToRead = strtok(paramClone, ",");
      AAIN(dirToRead, "wrong use of option w", stop = 1; error = 1; break;)

      char *sn = strtok(NULL, ",");
      int n = 0;

      if (sn && strlen(sn) >= 3 && sn[0] == 'n' && sn[1] == '=')
      {
        n = atoi(sn + 2);
      }

      // will contains files read using option w
      List_T readFiles = List_create(NULL, NULL, NULL, &error);
      AAIN(readFiles, "internal error during the handling of option w", puts(List_getErrorMessage(error)); error = 1;)

      if (!error)
      {
        AAINZ(readNFilesFromDir(dirToRead, &n, readFiles), "internal error during the handling of option w", error = 1)
      }

      const char *paramD = NULL;
      if (!error)
      {
        // check if -D was used as the next option
        if (p->next && p->next->op == 'D')
        {
          // skip it in the next iteration
          p = p->next;
          // retrieve the dirname where to store the evicted files
          paramD = p->param;
          if (paramD == NULL)
          {
            // it's not a big trouble, we can go on
            puts("Wrong usage of -D option: the argument is missing");
          }
        }
      }

      if (!error)
      {
        if (paramD == NULL)
        {
          paramD = ""; // forEach needs a non NULL context
        }
        List_forEachWithContext(readFiles, writeOptionCallback, (void *)paramD, &error);
      }

      readFiles ? List_free(&readFiles, 0, NULL) : (void)NULL;
      paramClone ? paramClone : NULL;

      if (error)
      {
        stop = 1;
      }

      break;
    }
    default:
    {
      break;
    }
    }
  }

  CommandLineParser_delete(&as, NULL);

  if (error)
  {
    return -1;
  }
  else
  {

    return 0;
  }
}