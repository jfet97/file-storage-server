#include <pthread.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "file-system.h"
#include "list.h"
#include "icl_hash.h"

#define SET_ERROR error && (*error = errToSet);
#define MAX_NUM_OF_BUCKETS 32768

#define NON_ZERO_DO(code, todo) \
    if (code != 0)              \
    {                           \
        todo;                   \
    }

#define IS_NULL_DO(code, todo) \
    if (code == NULL)          \
    {                          \
        todo;                  \
    }

struct File
{
    // also used as key for the FileSystem's files dict
    char *path;
    char *data;
    size_t size;

    OwnerId currentlyLockedBy; // OwnerId, 0 if no owner
    OwnerId ownerCanWrite;     // OwnerId, 0 if no owner
    List_T waitingLockers;     // OwnerIds
    List_T openedBy;           // OwnerIds

    size_t activeReaders;
    size_t activeWriters;

    pthread_mutex_t mutex;
    pthread_mutex_t ordering;
    pthread_cond_t go;
};

struct EvictedFile
{
    // also used as key for the FileSystem's files dict
    char *path;
    char *data;
    size_t size;
};

void fileDeallocator(void *rawFile)
{
    File file = rawFile;
    free(file->path);
    free(file->data);
    List_free(&file->currentlyLockedBy, 0, NULL);
    List_free(&file->waitingLockers, 0, NULL);
}

struct FileSystem
{
    size_t maxStorageSize;
    size_t maxNumOfFiles;
    size_t currentStorageSize;
    size_t currentNumOfFiles;
    int replacementPolicy;

    List_T filesList;
    // key: File.path, value: File
    icl_hash_t *filesDict;

    pthread_mutex_t overallMutex;
};

int ownerIdComparator(void *rawO1, void *rawO2)
{
    OwnerId *o1 = rawO1;
    OwnerId *o2 = rawO2;

    return o1->id == o2->id;
}

FileSystem FileSystem_create(size_t maxStorageSize, size_t maxNumOfFiles, int replacementPolicy, int *error)
{
    int errToSet = 0;
    size_t bucketsNum = maxNumOfFiles > MAX_NUM_OF_BUCKETS ? MAX_NUM_OF_BUCKETS : maxNumOfFiles;
    FileSystem fs = NULL;

    if (replacementPolicy != FS_REPLACEMENT_FIFO && replacementPolicy != FS_REPLACEMENT_LRU)
    {
        errToSet = E_FS_UNKNOWN_REPL;
    }

    if (!errToSet)
    {
        fs = malloc(sizeof(*fs));

        IS_NULL_DO(fs, {
            errToSet = E_FS_MALLOC;
        })
    }

    if (!errToSet)
    {
        // the filesDict is responsible of freeing the File entries and the string keys
        // others ops are not needed
        fs->filesList = List_create(NULL, NULL, fileDeallocator, &errToSet);
    }

    if (!errToSet)
    {
        // the default hash and compare functions are enough for our needs
        // because keys have type string
        fs->filesDict = icl_hash_create(bucketsNum, NULL, NULL);

        IS_NULL_DO(fs->filesDict, {
            errToSet = E_FS_MALLOC;
        })
    }

    if (!errToSet)
    {
        fs->maxNumOfFiles = maxNumOfFiles;
        fs->maxStorageSize = maxStorageSize;
        fs->replacementPolicy = replacementPolicy;
        fs->currentStorageSize = 0;
        fs->currentNumOfFiles = 0;

        NON_ZERO_DO(pthread_mutex_init(&fs->overallMutex, NULL), {
            errToSet = E_FS_MUTEX;
        })
    }

    if (errToSet)
    {
        fs && (fs->filesDict) ? icl_hash_destroy(fs->filesDict, NULL, NULL) : NULL;
        fs && (fs->filesList) ? List_free(&fs->filesList, 0, NULL) : NULL;
        fs ? free(fs) : NULL;
    }

    SET_ERROR;
    return fs;
}

void FileSystem_delete(FileSystem *fsPtr, int *error)
{
    // precondition: it will be called when all but one threads have died => no mutex is required

    int errToSet = 0;
    FileSystem fs = NULL;

    if (fsPtr == NULL || *fsPtr == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    if (!errToSet)
    {
        fs = *fsPtr;

        // the filesDict is not responsible of freeing the File entries nor the string keys
        icl_hash_destroy(fs->filesDict, NULL, NULL);

        // the list is
        List_free(&fs->filesList, 1, NULL);
        free(fs);
        *fsPtr = NULL;
    }

    SET_ERROR;
}

EvictedFile FileSystem_evict(FileSystem fs, int *error)
{
    // precondition: it will be called having the overallMutex and with a valid fs
    // the file list is not empty

    int errToSet = 0;
    int hasMutex = 0;
    int hasOrdering = 1;
    File file = List_extractTail(fs->filesList, &errToSet);
    EvictedFile evictedFile = NULL;

    if (!errToSet)
    {
        // remove the file from the dict as well
        icl_hash_delete(fs->filesDict, file->path, NULL, NULL);
        // update fs
        fs->currentNumOfFiles--;
        fs->currentStorageSize -= file->size;

        NON_ZERO_DO(pthread_mutex_lock(&file->ordering), {
            errToSet = E_FS_MUTEX;
            hasOrdering = 0;
        })
    }

    if (!errToSet)
    {
        hasMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex), {
            errToSet = E_FS_MUTEX;
            hasMutex = 0;
        })
    }

    if (!errToSet)
    {
        while (file->activeReaders > 0 || file->activeWriters > 0 || !errToSet)
        {
            NON_ZERO_DO(pthread_cond_wait(&file->go, &file->mutex), {
                errToSet = E_FS_COND;
            })
        }
    }

    if (!errToSet)
    {
        evictedFile = malloc(sizeof(*evictedFile));
        IS_NULL_DO(evictedFile, {
            errToSet = E_FS_MALLOC;
        })
    }

    if (!errToSet)
    {
        evictedFile->data = file->data;
        evictedFile->path = file->path;
        evictedFile->size = file->size;

        List_free(file->openedBy, 1, NULL);
        List_free(file->waitingLockers, 1, NULL);
        free(file);
    }

    SET_ERROR;
    return evictedFile;
}

EvictedFile FileSystem_openFile(FileSystem fs, char *path, int flags, OwnerId ownerId, int *error)
{
    // if something goes wrong is caller responsibility to free path
    int errToSet = 0;
    int hasFSMutex = 0;
    int insertedIntoList = 0;
    int insertedIntoDict = 0;
    int isAlreadyThereFile = 0;
    int createFlag = flags & O_CREATE;
    int lockFlag = flags & O_LOCK;
    File file = NULL;
    EvictedFile evictedFile = NULL;

    if (fs == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    if (path == NULL || (flags != O_CREATE && flags != O_LOCK && flags != O_CREATE | O_LOCK && flags != 0b0))
    {
        errToSet = E_FS_INVALID_ARGUMENTS;
    }

    if (!errToSet)
    {
        hasFSMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&fs->overallMutex), {
            errToSet = E_FS_MUTEX;
            hasFSMutex = 0;
        })
    }

    if (!errToSet)
    {
        file = icl_hash_find(fs->filesDict, path);
        if (file != NULL && createFlag)
        {
            errToSet = E_FS_FILE_ALREADY_IN;
        }
        if (file == NULL && !createFlag)
        {
            errToSet = E_FS_FILE_NOT_FOUND;
        }

        if (file != NULL)
        {
            isAlreadyThereFile = 1;
        }
    }

    if (!errToSet && !isAlreadyThereFile)
    {
        // new file to create

        if (fs->currentNumOfFiles == fs->maxNumOfFiles)
        {
            evictedFile = FileSystem_evict(fs, &errToSet);
        }

        if (!errToSet)
        {
            file = malloc(sizeof(*file));

            IS_NULL_DO(file, {
                errToSet = E_FS_MALLOC;
            })
        }

        if (!errToSet)
        {
            file->path = path;
            file->activeReaders = 0;
            file->activeWriters = 0;
            file->openedBy = List_create(ownerIdComparator, NULL, NULL, &errToSet);
        }

        if (!errToSet)
        {
            file->waitingLockers = List_create(ownerIdComparator, NULL, NULL, &errToSet);
        }

        if (!errToSet)
        {
            NON_ZERO_DO(pthread_mutex_init(&file->mutex, NULL), {
                errToSet = E_FS_MUTEX;
            })
        }

        if (!errToSet)
        {
            NON_ZERO_DO(pthread_mutex_init(&file->ordering, NULL), {
                errToSet = E_FS_MUTEX;
            })
        }

        if (!errToSet)
        {
            NON_ZERO_DO(pthread_cond_init(&file->go, NULL), {
                errToSet = E_FS_COND;
            })
        }

        if (!errToSet)
        {
            List_insertHead(fs->filesList, file, &errToSet);
            insertedIntoList = errToSet ? 0 : 1;
        }

        if (!errToSet)
        {
            insertedIntoDict = 1;
            IS_NULL_DO(icl_hash_insert(fs->filesDict, file->path, file), {
                errToSet = E_FS_GENERAL;
                insertedIntoDict = 0;
            })
        }

        if (!errToSet)
        {
            file->ownerCanWrite.id = ownerId.id;
            fs->currentNumOfFiles++;
        }
    }

    if (errToSet && !isAlreadyThereFile)
    {
        if (insertedIntoDict)
        {
            icl_hash_delete(fs->filesDict, file->path, NULL, NULL);
        }

        if (insertedIntoList)
        {
            List_deleteHead(fs->filesList, NULL);
        }
    }

    if (!errToSet)
    {
        // set who has opened the file as entry of the openedBy list of the file,
        // eventually set currentLockedBy, eventually reset ownerCanWrite
        // to do if the file is new or if the file was already present

        int hasMutex = 0;
        int hasOrdering = 1;

        NON_ZERO_DO(pthread_mutex_lock(&file->ordering), {
            errToSet = E_FS_MUTEX;
            hasOrdering = 0;
        })

        if (!errToSet)
        {
            hasMutex = 1;
            NON_ZERO_DO(pthread_mutex_lock(&file->mutex), {
                errToSet = E_FS_MUTEX;
                hasMutex = 0;
            })
        }

        if (!errToSet) // I do have the overallMutex
        {
            hasFSMutex = 0;
            NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex), {
                errToSet = E_FS_MUTEX;
                hasFSMutex = 1;
            })
        }

        if (!errToSet)
        {
            while (file->activeReaders > 0 || file->activeWriters > 0 || !errToSet)
            {
                NON_ZERO_DO(pthread_cond_wait(&file->go, &file->mutex), {
                    errToSet = E_FS_COND;
                })
            }
        }

        if (!errToSet)
        {
            file->activeWriters++;
        }

        if (hasOrdering)
        {
            hasOrdering = 0;
            NON_ZERO_DO(pthread_mutex_unlock(&file->ordering), {
                errToSet = E_FS_MUTEX;
                hasOrdering = 1;
            })
        }

        if (hasMutex)
        {
            hasMutex = 0;
            NON_ZERO_DO(pthread_mutex_unlock(&file->mutex), {
                errToSet = E_FS_MUTEX;
                hasMutex = 1;
            })
        }

        // do stuff if it is all alright
        if (!errToSet)
        {
            OwnerId *oid = NULL;
            int insertIntoList = 0;

            if (file->currentlyLockedBy.id != ownerId.id)
            {
                errToSet = E_FS_FILE_IS_LOCKED;
            }

            if (!errToSet)
            {
                oid = malloc(sizeof(*oid));
                IS_NULL_DO(oid, { errToSet = E_FS_MALLOC; })
            }

            if (!errToSet)
            {
                oid->id = ownerId.id;
                List_insertHead(file->openedBy, oid, &errToSet);
            }

            if (!errToSet)
            {
                insertIntoList = 1;
            }

            if (!errToSet && isAlreadyThereFile)
            {
                file->ownerCanWrite.id = 0;
            }

            if (!errToSet && lockFlag)
            {
                file->currentlyLockedBy.id = ownerId.id;
            }

            if (errToSet)
            {
                if (oid && insertIntoList)
                {
                    List_extractHead(file->openedBy, NULL);
                }
                oid ? free(oid) : NULL;
            }
        }

        if (!hasMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND)
        {
            hasMutex = 1;
            NON_ZERO_DO(pthread_mutex_lock(&file->mutex), {
                errToSet = E_FS_MUTEX;
                hasMutex = 0;
            })
        }

        if (!errToSet)
        {
            file->activeWriters--;
        }

        if (hasMutex)
        {
            NON_ZERO_DO(pthread_cond_signal(&file->go), {
                errToSet = E_FS_COND;
            })

            hasMutex = 0;
            NON_ZERO_DO(pthread_mutex_unlock(&file->mutex), {
                errToSet = E_FS_MUTEX;
                hasMutex = 1;
            })
        }
    }

    if (hasFSMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND)
    {
        hasFSMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex), {
            errToSet = E_FS_MUTEX;
            hasFSMutex = 1;
        })
    }

    SET_ERROR;
    return evictedFile;
}