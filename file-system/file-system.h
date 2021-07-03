#include <stddef.h>
#include "list.h"

#define O_CREATE 0x01
#define O_LOCK 0x02

#define E_FS_MALLOC 11
#define E_FS_NULL_FS 12
#define E_FS_GENERAL 13
#define E_FS_MUTEX 14
#define E_FS_COND 15
#define E_FS_UNKNOWN_REPL 16
#define E_FS_EXCEEDED_FILES_NUM 17
#define E_FS_EXCEEDED_SIZE 18
#define E_FS_INVALID_ARGUMENTS 19
#define E_FS_FILE_ALREADY_IN 20
#define E_FS_FILE_NOT_FOUND 21
#define E_FS_FILE_IS_LOCKED 22
#define E_FS_FILE_NOT_OPENED 23
#define E_FS_FILE_NO_WRITE 24
#define E_FS_FILE_NOT_LOCKED 25
#define E_FS_FILE_ALREADY_LOCKED 26
#define E_FS_RESULTFILE_IS_NULL 27
#define E_FS_FAILED_EVICT 28

#define FS_REPLACEMENT_FIFO 100
#define FS_REPLACEMENT_LRU 101

typedef struct File *File;
typedef struct FileSystem *FileSystem;
typedef struct
{
    int id;
} OwnerId;

typedef struct
{
    char *path;
    char *data;
    size_t size;
} *ResultFile;

typedef struct
{
    int index;
    char **paths;
} *Paths;

int ownerIdComparator(void *, void *);


FileSystem FileSystem_create(size_t maxStorageSize, size_t maxNumOfFiles, int replacementPolicy, int *error);
void FileSystem_delete(FileSystem *fsPtr, int *error);
ResultFile FileSystem_openFile(FileSystem fs, char *path, int flags, OwnerId ownerId, int *error);
ResultFile FileSystem_readFile(FileSystem fs, char *path, OwnerId ownerId, int *error);
List_T FileSystem_readNFile(FileSystem fs, OwnerId ownerId, int N, int *error); // TODO
List_T FileSystem_appendToFile(FileSystem fs, char *path, char *content, size_t contentSize, OwnerId ownerId, int write, int *error);
void FileSystem_lockFile(FileSystem fs, char *path, OwnerId ownerId, int *error);
OwnerId* FileSystem_unlockFile(FileSystem fs, char *path, OwnerId ownerId, int *error);
void FileSystem_closeFile(FileSystem fs, char *path, OwnerId ownerId, int *error);
void FileSystem_removeFile(FileSystem fs, char *path, OwnerId ownerId, int *error); // TODO
void FileSystem_evictClient(FileSystem fs, OwnerId ownerId, int *error); // TODO
void ResultFile_free(ResultFile* rfPtr, int* error);
size_t ResultFile_getCurrentSizeInByte(FileSystem fs, int *error);
size_t ResultFile_getCurrentNumOfFiles(FileSystem fs, int *error);
Paths ResultFile_getStoredFilesPaths(FileSystem fs, int *error);
const char *FileSystem_getErrorMessage(int errorCode);