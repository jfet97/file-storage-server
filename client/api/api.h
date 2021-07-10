#ifndef _CLIENT_SERVER_API_
#define _CLIENT_SERVER_API_

#include <stddef.h>
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
#include <ctype.h>
#include <poll.h>
#include <dirent.h>
#include <stddef.h>
#include <sys/types.h>
#include "communication.h"
#include <stdarg.h>

// ACHTUNG: failed logical operation on the remote file-system (e.g. openFile, lockFile,...)
// are repoprted using EPERM error

// if it will be provided, this variabile will contain the path of the dir
// used to store the evicted file from the server when openFile is called (set using -O option)
extern char *homeDirEvictedFiles;

// if 1, log strings will be outputted to stdout
extern int allowPrints;

// make a relative path absolute (using cwd)
char *absolutify(const char *path);
// write a file on disk
int writeLocalFile(const char *path, const void *data, size_t dataLen, const char *dirname);
// read a file from disk
int readLocalFile(const char *path, void **bufPtr, size_t *size);

int openConnection(const char *sockname, int msec, const struct timespec abstime);
int closeConnection(const char *sockname);
int openFile(const char *pathname, int flags);
int readFile(const char *pathname, void **buf, size_t *size);
int writeFile(const char *pathname, const char *dirname);
int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname);
int lockFile(const char *pathname);
int unlockFile(const char *pathname);
int closeFile(const char *pathname);
int removeFile(const char *pathname);
int readNFiles(int N, const char *dirname);

#endif