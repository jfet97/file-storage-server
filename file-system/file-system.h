#include <stddef.h>

#define E_FS_MALLOC 1
#define E_FS_NULL_FS 2
#define E_FS_GENERAL 3
#define E_FS_MUTEX 4
#define E_FS_UNKNOWN_REPL 5

#define FS_REPLACEMENT_FIFO 100
#define FS_REPLACEMENT_LRU 101

typedef struct File* File;
typedef struct FileSystem* FileSystem;

FileSystem FileSystem_create(size_t maxStorageSize, size_t maxNumOfFiles, int replacementPolicy, int* error);
void FileSystem_delete(FileSystem *fsPtr, int *error, void (*freeExtraDataEntry)(void *));