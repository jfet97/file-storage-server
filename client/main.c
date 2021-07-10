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

int timeToWaitBetweenConnections = 0; // set using option t

typedef struct
{
  const char *dirname;
  void *content;
  size_t contentSize;
} AppendOptionCallbackContext;

#define AWAIT                                                              \
  {                                                                        \
    if (timeToWaitBetweenConnections)                                      \
    {                                                                      \
      struct timespec interval;                                            \
      interval.tv_sec = timeToWaitBetweenConnections / 1000;               \
      interval.tv_nsec = (timeToWaitBetweenConnections % 1000) * 1000000L; \
      nanosleep(&interval, NULL); /* if it fails is not a big deal */      \
    }                                                                      \
  }

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

  // convert back to NULL if the rawDirname is an empty string
  char *dirname = strcmp(rawDirname, "") == 0 ? NULL : rawDirname;
  char *file = rawFilePath;

  errno = 0;

  AWAIT
  // try to open the file with both flags
  openFile(file, O_CREATE | O_LOCK);

  // if the operation has ended successfully
  if (errno == 0)
  {
    AWAIT
    writeFile(file, dirname);
    // is a bad bad error only if it is different from EPERM
    if (errno != EPERM)
    {
      *error = errno;
    }
  }
  else
  {
    if (errno != EPERM)
    {
      *error = errno;
    }
  }
}

// used to read a list of files
static void readOptionCallback(void *rawDirname, void *rawFilePath, int *error)
{

  if (*error)
  {
    return;
  }

  // convert back to NULL if the rawDirname is an empty string
  char *dirname = strcmp(rawDirname, "") == 0 ? NULL : rawDirname;
  char *file = rawFilePath;

  errno = 0;

  // try to read the file
  void *buf = NULL;
  size_t size;
  AWAIT
  readFile(file, &buf, &size);

  // if the operation has ended successfully
  if (errno == 0)
  {
    // write the file on disk if dirname is not NULL
    if (dirname)
    {
      writeLocalFile(file, buf, size, dirname);
    }
  }
  else
  {
    if (errno != EPERM)
    {
      *error = errno;
    }
  }

  if (buf)
  {
    free(buf);
  }
}

// used to append on a list of files
static void appendOptionCallback(void *rawCtx, void *rawFilePath, int *error)
{

  if (*error)
  {
    return;
  }

  AppendOptionCallbackContext *ctx = rawCtx;

  // convert back to NULL if the rawDirname is an empty string
  char *file = rawFilePath;

  errno = 0;

  // try to append to the file
  AWAIT
  appendToFile(file, ctx->content, ctx->contentSize, ctx->dirname);

  // if the error is different from op. not permitted, report it
  if (errno != EPERM)
  {
    *error = errno;
  }
}

// used to lock a list of files
static void lockOptionCallback(void *rawFilePath, int *error)
{
  if (*error)
  {
    return;
  }

  char *file = rawFilePath;

  errno = 0;

  // try to lock the file
  AWAIT
  lockFile(file);

  // if the error is different from op. not permitted, report it
  if (errno != EPERM)
  {
    *error = errno;
  }
}

// used to close a list of files
static void closeOptionCallback(void *rawFilePath, int *error)
{
  if (*error)
  {
    return;
  }

  char *file = rawFilePath;

  errno = 0;

  // try to close the file
  AWAIT
  closeFile(file);

  // if the error is different from op. not permitted, report it
  if (errno != EPERM)
  {
    *error = errno;
  }
}

// used to unlock a list of files
static void unlockOptionCallback(void *rawFilePath, int *error)
{
  if (*error)
  {
    return;
  }

  char *file = rawFilePath;

  errno = 0;

  // try to unlock the file
  AWAIT
  unlockFile(file);

  // if the error is different from op. not permitted, report it
  if (errno != EPERM)
  {
    *error = errno;
  }
}

// used to remove a list of files
static void removeOptionCallback(void *rawFilePath, int *error)
{
  if (*error)
  {
    return;
  }

  char *file = rawFilePath;

  errno = 0;

  // try to remove the file
  AWAIT
  removeFile(file);

  // if the error is different from op. not permitted, report it
  if (errno != EPERM)
  {
    *error = errno;
  }
}

// read *nPtr files recursively from a directory
static int readNFilesFromDir(const char *dirname, int *nPtr, List_T readFiles)
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

  const char *socket = ""; // socket file used for communication with the server

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
             "  -a,\tAccepts a list of files. The first will be a local source from where remotely append on the others"
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
             "  -s,\tClose the specified files\n"
             "  -c,\tRemoves the specified files from the server\n"
             "  -p,\tPrints information about operation performed on the server\n",
             argv[0], argv[0]);
    }
  }

  // check if option f is the first option and check if options t and p are used only one time
  // set option t and option p
  int fTimes = 0;
  int pTimes = 0;
  int fFirst = 0;
  for (Option *p = asList; p && !stop; p = p->next)
  {
    char op = p->op;
    if (op == 'f')
    {
      fFirst = p == asList;
      fTimes++;
    }
    if (op == 'p')
    {
      pTimes++;

      allowPrints = 1;
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
    case 't':
    {
      // save the timeToWaitBetweenConnections time
      if (p->param)
      {
        timeToWaitBetweenConnections = atoi(p->param);
      }
      else
      {
        timeToWaitBetweenConnections = 0;
      }
      break;
    }
    case 'O':
    {
      // it can be null
      homeDirEvictedFiles = (char *)param;
      break;
    }
    case 'D':
    {
      puts("wrong usage of option D");
      break;
    }
    case 'd':
    {
      puts("wrong usage of option d");
      break;
    }
    case 'f':
    {
      if (!param)
      {
        puts("wrong usage of option f");
        error = 1;
        stop = 1;
        break;
      }

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

      if (!param)
      {
        puts("wrong usage of option w");
        error = 1;
        stop = 1;
        break;
      }

      // clone the param string
      char *paramClone = strdup(param);
      AAIN(paramClone, "strdup has failed during the handling of option w", stop = 1; error = 1; break;)

      char *dirToRead = strtok(paramClone, ",");
      AAIN(dirToRead, "wrong use of option w", stop = 1; error = 1; free(paramClone); break;)

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
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
          AAINZ(readNFilesFromDir(dirToRead, &n, readFiles), "internal error during the handling of option w", error = 1)
          AAINZ(chdir(cwd), "internal error during the handling of option w", error = 1)
        }
        else
        {
          perror("getcwd() error");
          error = 1;
        }
      }

      // check if -D was used as the next option
      const char *paramD = NULL;
      if (!error)
      {
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

      readFiles ? List_free(&readFiles, 1, NULL) : (void)NULL;
      paramClone ? free(paramClone) : (void)NULL;

      if (error && error != EPERM)
      {
        stop = 1;
      }

      break;
    }
    case 'W':
    {

      if (!param)
      {
        puts("wrong usage of option W");
        error = 1;
        stop = 1;
        break;
      }

      // clone the param string
      char *paramClone = strdup(param);
      AAIN(paramClone, "strdup has failed during the handling of option W", stop = 1; error = 1; break;)

      // will contains files read using option W
      List_T readFiles = List_create(NULL, NULL, NULL, &error);
      AAIN(readFiles, "internal error during the handling of option W", puts(List_getErrorMessage(error)); error = 1;)

      // get the files' paths to read
      if (!error)
      {
        const char *token = strtok(paramClone, ",");
        while (token && !error)
        {
          List_insertHead(readFiles, (void *)token, &error);
          if (error)
          {
            puts("internal error during the handling of option W");
          }
          else
          {
            token = strtok(NULL, ",");
          }
        }
      }

      // check if -D was used as the next option
      const char *paramD = NULL;
      if (!error)
      {
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
      paramClone ? free(paramClone) : (void)NULL;

      if (error && error != EPERM)
      {
        stop = 1;
      }

      break;
    }
    case 'r':
    {

      if (!param)
      {
        puts("wrong usage of option r");
        error = 1;
        stop = 1;
        break;
      }

      // clone the param string
      char *paramClone = strdup(param);
      AAIN(paramClone, "strdup has failed during the handling of option r", stop = 1; error = 1; break;)

      // will contains files to read using option r
      List_T toReadFiles = List_create(NULL, NULL, NULL, &error);
      AAIN(toReadFiles, "internal error during the handling of option r", puts(List_getErrorMessage(error)); error = 1;)

      // get the files' paths to read
      if (!error)
      {
        const char *token = strtok(paramClone, ",");
        while (token && !error)
        {
          List_insertHead(toReadFiles, (void *)token, &error);
          if (error)
          {
            puts("internal error during the handling of option r");
          }
          else
          {
            token = strtok(NULL, ",");
          }
        }
      }

      // check if -d was used as the next option
      const char *paramD = NULL;
      if (!error)
      {
        if (p->next && p->next->op == 'd')
        {
          // skip it in the next iteration
          p = p->next;
          // retrieve the dirname where to store the read files
          paramD = p->param;
          if (paramD == NULL)
          {
            // it's not a big trouble, we can go on
            puts("Wrong usage of -d option: the argument is missing");
          }
        }
      }

      if (!error)
      {
        if (paramD == NULL)
        {
          paramD = ""; // forEach needs a non NULL context
        }
        List_forEachWithContext(toReadFiles, readOptionCallback, (void *)paramD, &error);
      }

      toReadFiles ? List_free(&toReadFiles, 0, NULL) : (void)NULL;
      paramClone ? free(paramClone) : (void)NULL;

      if (error && error != EPERM)
      {
        stop = 1;
      }

      break;
    }
    case 'R':
    {
      int n = 0;
      if (param)
      {
        n = atoi(param);
      }

      // check if -d was used as the next option
      const char *paramD = NULL;

      if (p->next && p->next->op == 'd')
      {
        // skip it in the next iteration
        p = p->next;
        // retrieve the dirname where to store the read files
        paramD = p->param;
        if (paramD == NULL)
        {
          // it's not a big trouble, we can go on
          puts("Wrong usage of -d option: the argument is missing");
        }
      }

      // try to read N Files
      AWAIT
      int r = readNFiles(n, paramD);
      if (r < 0)
      {
        perror("something has gone wrong during handling of option R");
      }
      else
      {
        if (allowPrints)
        {
          printf("%d files has been read\n", r);
        }
      }

      break;
    }
    case 'l':
    {

      if (!param)
      {
        puts("wrong usage of option l");
        error = 1;
        stop = 1;
        break;
      }

      // clone the param string
      char *paramClone = strdup(param);
      AAIN(paramClone, "strdup has failed during the handling of option l", stop = 1; error = 1; break;)

      // will contains files to lock using option l
      List_T toLockFiles = List_create(NULL, NULL, NULL, &error);
      AAIN(toLockFiles, "internal error during the handling of option l", puts(List_getErrorMessage(error)); error = 1;)

      // get the files' paths
      if (!error)
      {
        const char *token = strtok(paramClone, ",");
        while (token && !error)
        {
          List_insertHead(toLockFiles, (void *)token, &error);
          if (error)
          {
            puts("internal error during the handling of option l");
          }
          else
          {
            token = strtok(NULL, ",");
          }
        }
      }

      if (!error)
      {
        List_forEach(toLockFiles, lockOptionCallback, &error);
      };

      if (error && error != EPERM)
      {
        stop = 1;
      }

      toLockFiles ? List_free(&toLockFiles, 0, NULL) : (void)NULL;
      paramClone ? free(paramClone) : (void)NULL;

      break;
    }
    case 'u':
    {

      if (!param)
      {
        puts("wrong usage of option u");
        error = 1;
        stop = 1;
        break;
      }

      // clone the param string
      char *paramClone = strdup(param);
      AAIN(paramClone, "strdup has failed during the handling of option u", stop = 1; error = 1; break;)

      // will contains files to unnock using option u
      List_T toUnlockFiles = List_create(NULL, NULL, NULL, &error);
      AAIN(toUnlockFiles, "internal error during the handling of option u", puts(List_getErrorMessage(error)); error = 1;)

      // get the files' paths
      if (!error)
      {
        const char *token = strtok(paramClone, ",");
        while (token && !error)
        {
          List_insertHead(toUnlockFiles, (void *)token, &error);
          if (error)
          {
            puts("internal error during the handling of option u");
          }
          else
          {
            token = strtok(NULL, ",");
          }
        }
      }

      if (!error)
      {
        List_forEach(toUnlockFiles, unlockOptionCallback, &error);
      };

      if (error && error != EPERM)
      {
        stop = 1;
      }

      toUnlockFiles ? List_free(&toUnlockFiles, 0, NULL) : (void)NULL;
      paramClone ? free(paramClone) : (void)NULL;

      break;
    }
    case 'c':
    {

      if (!param)
      {
        puts("wrong usage of option c");
        error = 1;
        stop = 1;
        break;
      }

      // clone the param string
      char *paramClone = strdup(param);
      AAIN(paramClone, "strdup has failed during the handling of option c", stop = 1; error = 1; break;)

      // will contains files to remove using option c
      List_T toRemoveFiles = List_create(NULL, NULL, NULL, &error);
      AAIN(toRemoveFiles, "internal error during the handling of option c", puts(List_getErrorMessage(error)); error = 1;)

      // get the files' paths
      if (!error)
      {
        const char *token = strtok(paramClone, ",");
        while (token && !error)
        {
          List_insertHead(toRemoveFiles, (void *)token, &error);
          if (error)
          {
            puts("internal error during the handling of option c");
          }
          else
          {
            token = strtok(NULL, ",");
          }
        }
      }

      if (!error)
      {
        List_forEach(toRemoveFiles, removeOptionCallback, &error);
      };

      if (error && error != EPERM)
      {
        stop = 1;
      }

      toRemoveFiles ? List_free(&toRemoveFiles, 0, NULL) : (void)NULL;
      paramClone ? free(paramClone) : (void)NULL;

      break;
    }
    case 's':
    {

      if (!param)
      {
        puts("wrong usage of option s");
        error = 1;
        stop = 1;
        break;
      }

      // clone the param string
      char *paramClone = strdup(param);
      AAIN(paramClone, "strdup has failed during the handling of option s", stop = 1; error = 1; break;)

      // will contains files to close using option s
      List_T toCloseFiles = List_create(NULL, NULL, NULL, &error);
      AAIN(toCloseFiles, "internal error during the handling of option s", puts(List_getErrorMessage(error)); error = 1;)

      // get the files' paths
      if (!error)
      {
        const char *token = strtok(paramClone, ",");
        while (token && !error)
        {
          List_insertHead(toCloseFiles, (void *)token, &error);
          if (error)
          {
            puts("internal error during the handling of option s");
          }
          else
          {
            token = strtok(NULL, ",");
          }
        }
      }

      if (!error)
      {
        List_forEach(toCloseFiles, closeOptionCallback, &error);
      };

      if (error && error != EPERM)
      {
        stop = 1;
      }

      toCloseFiles ? List_free(&toCloseFiles, 0, NULL) : (void)NULL;
      paramClone ? free(paramClone) : (void)NULL;

      break;
    }




    case 'a':
    {

      if (!param)
      {
        puts("wrong usage of option a");
        error = 1;
        stop = 1;
        break;
      }

      // clone the param string
      char *paramClone = strdup(param);
      AAIN(paramClone, "strdup has failed during the handling of option a", stop = 1; error = 1; break;)

      // will contains files to append on using option a
      List_T appFiles = List_create(NULL, NULL, NULL, &error);
      AAIN(appFiles, "internal error during the handling of option W", puts(List_getErrorMessage(error)); error = 1;)

      // the first file path is the on from which the content will be read to be appendend
      // on the other files on the list
      const char *from = NULL;
      if (!error)
      {
        from = strtok(paramClone, ",");

        const char *token = strtok(NULL, ",");
        if (token == NULL)
        {
          puts("wrong usage of option a, the list of files to append on is empty");
          error = 1;
        }

        while (token && !error)
        {
          List_insertHead(appFiles, (void *)token, &error);
          if (error)
          {
            puts("internal error during the handling of option a");
          }
          else
          {
            token = strtok(NULL, ",");
          }
        }
      }

      // check if -D was used as the next option
      const char *paramD = NULL;
      if (!error)
      {
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

      void *fromContent = NULL;
      size_t fromSize = 0;
      if (!error)
      {
        int r = readLocalFile(from, &fromContent, &fromSize);
        if (r == -1)
        {
          perror("internal error during the handling of option a");
          error = 1;
        }
      }

      if (!error)
      {
        AppendOptionCallbackContext ctx;
        ctx.content = fromContent;
        ctx.contentSize = fromSize;
        ctx.dirname = paramD;
        List_forEachWithContext(appFiles, appendOptionCallback, &ctx, &error);
      }

      appFiles ? List_free(&appFiles, 0, NULL) : (void)NULL;
      paramClone ? free(paramClone) : (void)NULL;
      fromContent ? free(fromContent) : (void)NULL;

      if (error && error != EPERM)
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
  if (allowPrints)
  {
    puts("Goodbye!");
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