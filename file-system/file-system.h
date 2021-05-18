#include <stddef.h>

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

#define FS_REPLACEMENT_FIFO 100
#define FS_REPLACEMENT_LRU 101

#define O_CREATE 0b01
#define O_LOCK 0b10

typedef struct File *File;
typedef struct ResultFile *ResultFile;
typedef struct FileSystem *FileSystem;
typedef struct
{
    int id;
} OwnerId;

int ownerIdComparator(void *, void *);


FileSystem FileSystem_create(size_t maxStorageSize, size_t maxNumOfFiles, int replacementPolicy, int *error);
void FileSystem_delete(FileSystem *fsPtr, int *error);
ResultFile FileSystem_openFile(FileSystem fs, char *path, int flags, OwnerId ownerId, int *error);
ResultFile FileSystem_readFile(FileSystem fs, char *path, OwnerId ownerId, int *error);
List_T FileSystem_readNFile(FileSystem fs, OwnerId ownerId, int N, int *error);
List_T FileSystem_appendToFile(FileSystem fs, char *path, char *content, size_t contentSize, OwnerId ownerId, int write, int *error);