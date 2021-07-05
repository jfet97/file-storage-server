#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
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

#define IS_NULL_DO_ELSE(cond, todo, todoelse) \
    if (cond == NULL)                         \
    {                                         \
        todo;                                 \
    }                                         \
    else                                      \
    {                                         \
        todoelse;                             \
    }

#define TO_GENERAL_ERROR(E) \
    E ? E = E_FS_GENERAL : 0;

struct File
{
    // also used as key for the FileSystem's files dict
    char *path;
    char *data;
    size_t size;

    OwnerId currentlyLockedBy; // OwnerId (client id), 0 if no owner
    OwnerId ownerCanWrite;     // OwnerId (client id), 0 if no owner
    List_T waitingLockers;     // OwnerIds (client ids)
    List_T openedBy;           // OwnerIds (client ids)

    size_t activeReaders;
    size_t activeWriters;

    pthread_mutex_t mutex;
    pthread_mutex_t ordering;
    pthread_cond_t go;
};

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

    size_t maxNumOfFilesReached;
    size_t maxByteOfStorageUsed;
    size_t numOfReplacementAlgoRuns;
};

struct ReadNFileSetResultFilesContext
{
    List_T resultFiles;
    int counter;
    OwnerId ownerId;
};

struct ReadNFileLRUContext
{
    List_T files;
};

struct EvictClientContext
{
    OwnerId oid;
    List_T oidsHaveLocks;
};

// Callback used to print a file data, use only in the FileSystem_delete function
// or in a single threaded environment
void printFile(void *rawFile, int *_)
{
    File file = rawFile;
    puts("--------------------------");
    printf("File Path is: %s\n", file->path);
    printf("File Data is: %s\n", file->data);
    printf("File Size is: %d\n", file->size);
    puts("--------------------------");
}

// Callback used to free a list of files
void fileDeallocator(void *rawFile)
{
    File file = rawFile;
    free(file->path);
    free(file->data);
    // free also the ownerIds inside the lists
    List_free(&(file->waitingLockers), 1, NULL);
    List_free(&(file->openedBy), 1, NULL);
}

// Callback used to free a list of resultFiles
void fileResultDeallocator(void *rawFile)
{
    ResultFile file = rawFile;
    free(file->path);
    free(file->data);
}

// Callback used to compare two OwnerIds
int ownerIdComparator(void *rawO1, void *rawO2)
{
    OwnerId *o1 = rawO1;
    OwnerId *o2 = rawO2;

    return o1->id == o2->id;
}

// Callback used to compare two files' paths
int filePathComparator(void *rawf1, void *rawf2)
{
    File f1 = rawf1;
    File f2 = rawf2;

    return strcmp(f1->path, f2->path) == 0;
}

// Callback used to extract the paths of a list of file
//
// Note: the caller must own the file-system mutex
void extractPaths(void *rawPaths, void *rawFile, int *error)
{
    // precondition: has overall mutex
    Paths ps = rawPaths;
    File file = rawFile;

    if (!(*error))
    {
        memcpy(ps->paths + ps->index++, &file->path, sizeof(file->path));
    }
}

// Callback used to evict a client from the structures of a file
//
// Note: the caller must own the file-system lock
void evictClientInternal(void *rawCtx, void *rawFile, int *error)
{
    struct EvictClientContext *ctx = rawCtx;
    OwnerId *oidToFree = NULL;
    OwnerId *oidToGiveLock = NULL;
    File file = rawFile;
    int hasMutex = 0;
    int hasOrdering = 0;

    // acquire the file's mutexes
    if (!(*error))
    {
        hasOrdering = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->ordering),
                    {
                        *error = E_FS_MUTEX;
                        hasOrdering = 0;
                    })
    }

    if (!(*error))
    {
        hasMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        *error = E_FS_MUTEX;
                        hasMutex = 0;
                    })
    }

    if (!(*error))
    {
        while ((file->activeReaders > 0 || file->activeWriters > 0) && !(*error))
        {
            NON_ZERO_DO(pthread_cond_wait(&file->go, &file->mutex), {
                *error = E_FS_COND;
            })
        }
    }

    // this function acts as a writer
    if (!(*error))
    {
        file->activeWriters++;
    }

    // unlock the file's mutexes
    if (hasOrdering)
    {
        hasOrdering = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->ordering),
                    {
                        *error = E_FS_MUTEX;
                        hasOrdering = 1;
                    })
    }

    if (hasMutex)
    {
        hasMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->mutex),
                    {
                        *error = E_FS_MUTEX;
                        hasMutex = 1;
                    })
    }

    // if no error has occurred
    if (!(*error))
    {
        // unlock the file if it was locked by the ownerId to evict
        if (file->currentlyLockedBy.id == ctx->oid.id)
        {
            file->currentlyLockedBy.id = 0;
            oidToGiveLock = List_extractHead(file->waitingLockers, error);
        }

        // eventually reset the write flag
        if (!(*error) && file->ownerCanWrite.id == ctx->oid.id)
        {

            file->ownerCanWrite.id = 0;
        }

        // replace the locker if there were anyone else in the waiting list
        if (!(*error) && oidToGiveLock)
        {
            file->currentlyLockedBy.id = oidToGiveLock->id;
            // return the id that has gained the file's logic lock to the caller
            List_insertHead(ctx->oidsHaveLocks, oidToGiveLock, error);
        }

        if (!(*error))
        {
            // search the ownerId inside the openedBy list and extract it...
            oidToFree = List_searchExtract(file->openedBy, ownerIdComparator, &(ctx->oid), error);
        }

        if (!(*error))
        {
            // ...free it if it was found...
            (oidToFree ? free(oidToFree) : (void)NULL);
            oidToFree = NULL;
            // ...then search it inside the waitingLockers list...
            oidToFree = List_searchExtract(file->waitingLockers, ownerIdComparator, &(ctx->oid), error);
        }

        if (!(*error))
        {
            // ...free it if it was found
            (oidToFree ? free(oidToFree) : (void)NULL);
        }
    }

    // acquire the file mutex
    if (!(*error))
    {
        hasMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        *error = E_FS_MUTEX;
                        hasMutex = 0;
                    })
    }

    // work is done
    if ((*error) != E_FS_MUTEX && (*error) != E_FS_COND)
    {
        file->activeWriters--;
    }

    // release the file's lock
    if (hasMutex)
    {
        hasMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->mutex),
                    {
                        *error = E_FS_MUTEX;
                        hasMutex = 1;
                    })
    }
}

// Wrapper for the standard realloc function
int realloca(char **buf, size_t newsize)
{
    int toRet = 0;

    void *newbuff = realloc(*buf, newsize);
    if (newbuff == NULL)
    {
        toRet = -1;
    }
    else
    {
        *buf = newbuff;
    }

    return toRet;
}

// Callback used to read at most N files from the file-system
//
// Notes:
// - the caller must have acquired the file-system mutex
// - skip those locked files that were locked by other clients
void readNFiles(void *rawCtx, void *rawFile, int *error)
{
    struct ReadNFileSetResultFilesContext *ctx = rawCtx;
    ResultFile resultFile = NULL;
    File file = rawFile;

    int hasMutex = 0;
    int hasOrdering = 0;

    int isLockedByOthers = 0;
    int activeReadersUpdated = 0;

    // check arguments and decrease the counter
    if (ctx->counter <= 0 || *error)
    {
        return;
    }
    else
    {
        ctx->counter--;
    }

    // acquire the file's locks
    if (!(*error))
    {
        hasOrdering = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->ordering),
                    {
                        *error = E_FS_MUTEX;
                        hasOrdering = 0;
                    })
    }

    if (!(*error))
    {
        hasMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        *error = E_FS_MUTEX;
                        hasMutex = 0;
                    })
    }

    if (!(*error))
    {
        while (file->activeWriters > 0 && !(*error))
        {
            NON_ZERO_DO(pthread_cond_wait(&file->go, &file->mutex), {
                *error = E_FS_COND;
            })
        }
    }

    // this function acts as a writer
    if (!(*error))
    {
        file->activeReaders++;
        activeReadersUpdated = 1;
    }

    // release file's locks
    if (hasOrdering)
    {
        hasOrdering = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->ordering),
                    {
                        *error = E_FS_MUTEX;
                        hasOrdering = 1;
                    })
    }

    if (hasMutex)
    {
        hasMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->mutex),
                    {
                        *error = E_FS_MUTEX;
                        hasMutex = 1;
                    })
    }

    // if the file was locked by someone else, restore the counter and do nothing else
    if (!(*error))
    {
        isLockedByOthers = file->currentlyLockedBy.id != 0 && file->currentlyLockedBy.id != ctx->ownerId.id;
        if (isLockedByOthers)
        {
            ctx->counter++;
        }
    }

    // otherwise create a resultFile structure
    if (!(*error) && !isLockedByOthers)
    {
        resultFile = malloc(sizeof(*resultFile));
        IS_NULL_DO(resultFile, { *error = E_FS_MALLOC; })
    }

    // insert into the resultFile structure the file's data
    if (!(*error) && !isLockedByOthers)
    {
        resultFile->size = file->size;
        resultFile->data = malloc(sizeof(*resultFile->data) * file->size);
        resultFile->path = malloc(sizeof(*resultFile->path) * (strlen(file->path) + 1));
        IS_NULL_DO(resultFile->data, { *error = E_FS_MALLOC; })
        IS_NULL_DO(resultFile->path, { *error = E_FS_MALLOC; })
    }

    if (!(*error) && !isLockedByOthers)
    {
        memcpy(resultFile->data, file->data, file->size);
        memcpy(resultFile->path, file->path, strlen(file->path) + 1);
    }

    if (!(*error) && !isLockedByOthers)
    {
        List_insertHead(ctx->resultFiles, resultFile, error);
    }

    // in case of error, destroy the resultFile that may be incosistent
    if (*error && resultFile)
    {
        resultFile->data ? free(resultFile->data) : (void)NULL;
        resultFile->path ? free(resultFile->path) : (void)NULL;
        free(resultFile);
    }

    // acquire the mutex lock
    if (!(*error))
    {
        hasMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        *error = E_FS_MUTEX;
                        hasMutex = 0;
                    })
    }

    if ((*error) != E_FS_MUTEX && (*error) != E_FS_COND)
    {
        if (activeReadersUpdated)
        {
            file->activeReaders--;

            if (file->activeReaders == 0)
            {
                NON_ZERO_DO(pthread_cond_signal(&file->go), {
                    *error = E_FS_COND;
                })
            };
        }
    }

    // release the file mutex
    if (hasMutex)
    {
        hasMutex = 0;

        NON_ZERO_DO(pthread_mutex_unlock(&file->mutex),
                    {
                        *error = E_FS_MUTEX;
                        hasMutex = 1;
                    })
    }
}

// Callback used to put on top of the file-system list a list of files
//
// Note: it is used in a context where the file-system mutex was previously acquired
void moveFilesLRU(void *rawCtx, void *rawResultFile, int *error)
{
    if (*error)
    {
        return;
    }

    struct ReadNFileLRUContext *ctx = rawCtx;
    ResultFile resultFile = rawResultFile;

    struct File temp;
    temp.path = resultFile->path;

    List_insertHead(ctx->files, List_searchExtract(ctx->files, filePathComparator, &temp, NULL), error);
}

// Create a new file-system
FileSystem FileSystem_create(size_t maxStorageSize, size_t maxNumOfFiles, int replacementPolicy, int *error)
{
    int errToSet = 0;
    size_t bucketsNum = maxNumOfFiles > MAX_NUM_OF_BUCKETS ? MAX_NUM_OF_BUCKETS : maxNumOfFiles;
    FileSystem fs = NULL;

    // chech the replacementPolicy argument
    if (replacementPolicy != FS_REPLACEMENT_FIFO && replacementPolicy != FS_REPLACEMENT_LRU)
    {
        errToSet = E_FS_UNKNOWN_REPL;
    }

    // alloc a new file-system
    if (!errToSet)
    {
        fs = malloc(sizeof(*fs));

        IS_NULL_DO(fs, {
            errToSet = E_FS_MALLOC;
        })
    }

    // create the internal list of files, mainly used for the replacement policies
    if (!errToSet)
    {
        fs->filesList = List_create(NULL, NULL, fileDeallocator, &errToSet);
        TO_GENERAL_ERROR(errToSet);
    }

    // create the internal dictionary of files, it enables a faster search
    if (!errToSet)
    {
        // the default hash and compare functions are enough for our needs
        // because keys have type string
        fs->filesDict = icl_hash_create(bucketsNum, NULL, NULL);

        IS_NULL_DO(fs->filesDict, {
            errToSet = E_FS_MALLOC;
        })
    }

    // set the file system
    if (!errToSet)
    {
        fs->maxNumOfFiles = maxNumOfFiles;
        fs->maxStorageSize = maxStorageSize;
        fs->replacementPolicy = replacementPolicy;
        fs->maxNumOfFilesReached = 0;
        fs->maxByteOfStorageUsed = 0;
        fs->numOfReplacementAlgoRuns = 0;
        fs->currentStorageSize = 0;
        fs->currentNumOfFiles = 0;

        NON_ZERO_DO(pthread_mutex_init(&fs->overallMutex, NULL), {
            errToSet = E_FS_MUTEX;
        })
    }

    // in case of error destroy all
    if (errToSet)
    {
        fs && (fs->filesDict) ? icl_hash_destroy(fs->filesDict, NULL, NULL) : 0;
        fs && (fs->filesList) ? List_free(&fs->filesList, 0, NULL) : (void)NULL;
        fs ? free(fs) : (void)NULL;
    }

    SET_ERROR;
    return fs;
}

// Destroy a file-system
// Note: this function must be called only when all but the main one threads have died => no mutex is required
void FileSystem_delete(FileSystem *fsPtr, int *error)
{

    int errToSet = 0;
    FileSystem fs = NULL;

    // arguments check
    if (fsPtr == NULL || *fsPtr == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    // print stats, then free what has to be freed
    if (!errToSet)
    {
        fs = *fsPtr;

        puts("\nFILE SYSTEM FINAL STATS");
        printf("Max number of file stored was: %d\n", fs->maxNumOfFilesReached);
        printf("Max Mbytes of space used was: %d\n", fs->maxByteOfStorageUsed / 1024);
        printf("Number of times the replacing algoritm has run was: %d\n", fs->numOfReplacementAlgoRuns);
        puts("Follows a list of file still present in the file-system");
        List_forEach(fs->filesList, printFile, NULL);
        puts("END...HAVE A NICE DAY!");

        // the filesDict is not responsible of freeing the File entries nor the string keys
        icl_hash_destroy(fs->filesDict, NULL, NULL);

        // the list is
        List_free(&fs->filesList, 1, NULL);
        free(fs);
        *fsPtr = NULL;
    }

    SET_ERROR;
}

// Evict a file from the file-system
//
// Notes:
// - if the first extracted file is equal to path, another file should be evicted in its place
// - the path is owned by the caller
// - precondition: must be called with the overall (file-system) mutex already taken
ResultFile FileSystem_evict(FileSystem fs, char *path, int *error)
{
    int errToSet = 0;
    File file = NULL;
    File temp = NULL;
    ResultFile evictedFile = NULL;
    size_t listLength = List_length(fs->filesList, &errToSet);
    TO_GENERAL_ERROR(errToSet);
    int hasMutex = 0;
    int hasOrdering = 0;
    int hasExtractedFile = 0;

    // try to pick a file from the end of the list
    if (!errToSet)
    {
        file = List_pickTail(fs->filesList, &errToSet);
        IS_NULL_DO(file, { errToSet = E_FS_FAILED_EVICT; })
    }

    // if the picked file's path corresponds to the path argument, but there are no other files to evict
    // in its place, declare a failure
    if (!errToSet && path && strncmp(file->path, path, strlen(path)) == 0 && listLength - 1 == 0)
    {
        errToSet = E_FS_FAILED_EVICT;
    }

    // if the picked file's path corresponds to the path argument, and there is at least one file to evict
    // in its place, evict it
    if (!errToSet && path && strncmp(file->path, path, strlen(path)) == 0 && listLength - 1 > 0)
    {
        // temp: the file to not be evicted; it was previously picked
        // will be inserted back
        temp = List_extractTail(fs->filesList, &errToSet);
        TO_GENERAL_ERROR(errToSet);

        if (!errToSet)
        {
            // file: the file to evict
            file = List_extractTail(fs->filesList, &errToSet);
            TO_GENERAL_ERROR(errToSet);
            if (!errToSet)
            {
                hasExtractedFile = 1;
            }
        }

        if (!errToSet)
        {
            // reinsert temp
            List_insertTail(fs->filesList, temp, &errToSet);
            TO_GENERAL_ERROR(errToSet);
        }

        if (errToSet)
        {
            errToSet = E_FS_FAILED_EVICT;
        }
        else
        {
            temp = NULL;
        }
    }
    else
    {
        // if the picket file's path does not correspond to the path argument, or path was NULL, evict the picked file
        if ((!errToSet && path && strncmp(file->path, path, strlen(path)) != 0) || !path)
        {
            file = List_extractTail(fs->filesList, &errToSet);
            TO_GENERAL_ERROR(errToSet);
            if (!errToSet)
            {
                hasExtractedFile = 1;
            }
        }
    }

    if (!errToSet)
    {

        hasOrdering = 1;
        // acquire the ordering lock of the evicted file
        NON_ZERO_DO(pthread_mutex_lock(&file->ordering),
                    {
                        errToSet = E_FS_MUTEX;
                        hasOrdering = 0;
                    })
    }

    if (!errToSet)
    {
        hasMutex = 1;
        // acquire the mutex lock of the evicted file
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 0;
                    })
    }

    if (!errToSet)
    {
        // wait my turn
        while (!errToSet && (file->activeReaders > 0 || file->activeWriters > 0))
        {
            NON_ZERO_DO(pthread_cond_wait(&file->go, &file->mutex), {
                errToSet = E_FS_COND;
            })
        }
    }

    if (!errToSet)
    {
        // remove the file from the dict as well
        icl_hash_delete(fs->filesDict, file->path, NULL, NULL);

        // create a ResultFile structure to be returned
        evictedFile = malloc(sizeof(*evictedFile));
        IS_NULL_DO(evictedFile, {
            errToSet = E_FS_MALLOC;
        })
    }

    if (!errToSet)
    {
        // update the file-system
        fs->currentNumOfFiles--;
        fs->currentStorageSize -= file->size;

        // fulfill the evictedFile structure
        evictedFile->data = file->data;
        evictedFile->path = file->path;
        evictedFile->size = file->size;

        fs->numOfReplacementAlgoRuns++;
    }

    // always free all the file's data
    file &&hasExtractedFile ? List_free(&file->openedBy, 1, NULL) : (void)NULL;
    file &&hasExtractedFile ? List_free(&file->waitingLockers, 1, NULL) : (void)NULL;
    file &&hasExtractedFile ? free(file) : (void)NULL;
    temp ? List_free(&temp->openedBy, 1, NULL) : (void)NULL;
    temp ? List_free(&temp->waitingLockers, 1, NULL) : (void)NULL;
    temp ? free(temp) : (void)NULL;

    if (errToSet && hasMutex)
    {
        NON_ZERO_DO(pthread_mutex_unlock(&file->mutex), errToSet = E_FS_MUTEX;)
    }

    if (errToSet && hasOrdering)
    {
        NON_ZERO_DO(pthread_mutex_unlock(&file->ordering), errToSet = E_FS_MUTEX;)
    }

    SET_ERROR;
    return evictedFile;
}

// Open a file.
//
// Note: the path argument is owned by the caller, so it will be cloned
ResultFile FileSystem_openFile(FileSystem fs, char *path, int flags, OwnerId ownerId, int *error)
{
    int errToSet = 0;
    int hasFSMutex = 0; // overall (file-system) mutex
    int insertedIntoList = 0;
    int insertedIntoDict = 0;
    int isAlreadyThereFile = 0; // is the file already present in the file-system?
    int createFlag = flags & O_CREATE;
    int lockFlag = flags & O_LOCK;
    File file = NULL;
    ResultFile evictedFile = NULL;

    // check arguments
    if (fs == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    if (!errToSet && (path == NULL || (flags != O_CREATE && flags != O_LOCK && flags != (O_CREATE | O_LOCK) && flags != 0x00)))
    {
        errToSet = E_FS_INVALID_ARGUMENTS;
    }

    // get overall mutex
    if (!errToSet)
    {
        hasFSMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 0;
                    })
    }

    // check if the file is already present in the file-system,
    // to then act properly according to the received flags
    if (!errToSet)
    {
        file = icl_hash_find(fs->filesDict, path);

        // if the file was there, the O_CREATE flag must be set to 0 to proceed
        if (file != NULL && createFlag)
        {
            errToSet = E_FS_FILE_ALREADY_IN;
        }

        // if the file was not there, the O_CREATE flag must be set to 1 to proceed
        if (file == NULL && !createFlag)
        {
            errToSet = E_FS_FILE_NOT_FOUND;
        }

        if (file != NULL)
        {
            isAlreadyThereFile = 1;
        }
    }

    // in case of LRU policy, put the file (if it already exists) on top of the list
    if (!errToSet && isAlreadyThereFile && fs->replacementPolicy == FS_REPLACEMENT_LRU)
    {
        struct File temp;
        temp.path = file->path;
        List_insertHead(fs->filesList, List_searchExtract(fs->filesList, filePathComparator, &temp, NULL), &errToSet);
        TO_GENERAL_ERROR(errToSet);

        if (errToSet)
        {
            errToSet = E_FS_GENERAL;
        }
    }

    // enter here iff the file has to be created first
    if (!errToSet && !isAlreadyThereFile)
    {

        // check if the number of file limit was reached
        if (fs->currentNumOfFiles == fs->maxNumOfFiles)
        {
            // in such a case a file must be evicted
            evictedFile = FileSystem_evict(fs, NULL, &errToSet);
        }

        // allocate a new file
        if (!errToSet)
        {
            file = malloc(sizeof(*file));

            IS_NULL_DO(file, {
                errToSet = E_FS_MALLOC;
            })
        }

        // initialize it
        if (!errToSet)
        {
            file->data = NULL;
            file->size = 0;
            file->activeReaders = 0;
            file->activeWriters = 0;
            file->currentlyLockedBy.id = 0;
            // there is nothing that should be manually freed
            file->openedBy = List_create(ownerIdComparator, NULL, NULL, &errToSet);
            TO_GENERAL_ERROR(errToSet);
        }

        if (!errToSet)
        {
            file->path = malloc(sizeof(char) * (strlen(path) + 1));
            IS_NULL_DO(file->path, { errToSet = E_FS_MALLOC; })
        }

        if (!errToSet)
        {
            strcpy(file->path, path);
            // there is nothing that should be manually freed
            file->waitingLockers = List_create(ownerIdComparator, NULL, NULL, &errToSet);
            TO_GENERAL_ERROR(errToSet);
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

        // if the initialization has endend correctly, insert the file inside the file-system
        if (!errToSet)
        {
            List_insertHead(fs->filesList, file, &errToSet);
            TO_GENERAL_ERROR(errToSet);
            insertedIntoList = errToSet ? 0 : 1;
        }

        if (!errToSet)
        {
            insertedIntoDict = 1;
            IS_NULL_DO(icl_hash_insert(fs->filesDict, file->path, file),
                       {
                           errToSet = E_FS_GENERAL;
                           insertedIntoDict = 0;
                       })
        }

        if (!errToSet)
        {
            // the client is able to write on the file only if the previous operation was the open one
            // with both the flags enabled
            file->ownerCanWrite.id = createFlag && lockFlag ? ownerId.id : 0;
            fs->currentNumOfFiles++;
            fs->maxNumOfFilesReached = fs->currentNumOfFiles > fs->maxNumOfFilesReached ? fs->currentNumOfFiles : fs->maxNumOfFilesReached;
        }
    }

    // enters here if an error has occurred during the creation of a new file
    //
    // note: the evicted file won't be reinserted back into the file-system
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

        file && file->path ? free(file->path) : (void)NULL;
        file && file->openedBy ? List_free(&file->openedBy, 0, NULL) : (void)NULL;
        file && file->waitingLockers ? List_free(&file->waitingLockers, 0, NULL) : (void)NULL;
        file ? free(file) : (void)NULL;
        file = NULL;
    }

    // enters here whether the file had to be created from scratch or not
    if (!errToSet)
    {
        // these flag are about the file's mutexes
        int hasMutex = 0;
        int hasOrdering = 1;

        // get the file's locks
        NON_ZERO_DO(pthread_mutex_lock(&file->ordering),
                    {
                        errToSet = E_FS_MUTEX;
                        hasOrdering = 0;
                    })

        if (!errToSet)
        {
            hasMutex = 1;
            NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                        {
                            errToSet = E_FS_MUTEX;
                            hasMutex = 0;
                        })
        }

        // the overall mutex is not needed anymore
        if (!errToSet)
        {
            hasFSMutex = 0;
            NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex),
                        {
                            errToSet = E_FS_MUTEX;
                            hasFSMutex = 1;
                        })
        }

        if (!errToSet)
        {
            while (!errToSet && (file->activeReaders > 0 || file->activeWriters > 0))
            {
                NON_ZERO_DO(pthread_cond_wait(&file->go, &file->mutex), {
                    errToSet = E_FS_COND;
                })
            }
        }

        // this function is considered as a writer of the file
        if (!errToSet)
        {
            file->activeWriters++;
        }

        // release file's locks
        if (hasOrdering)
        {
            hasOrdering = 0;
            NON_ZERO_DO(pthread_mutex_unlock(&file->ordering),
                        {
                            errToSet = E_FS_MUTEX;
                            hasOrdering = 1;
                        })
        }

        if (hasMutex)
        {
            hasMutex = 0;
            NON_ZERO_DO(pthread_mutex_unlock(&file->mutex),
                        {
                            errToSet = E_FS_MUTEX;
                            hasMutex = 1;
                        })
        }

        if (!errToSet)
        {
            OwnerId *oid = NULL;
            int insertIntoList = 0;

            // if the file was locked by someone else, it cannot be opened
            if (file->currentlyLockedBy.id != 0 && file->currentlyLockedBy.id != ownerId.id)
            {
                errToSet = E_FS_FILE_IS_LOCKED;
            }

            // else, if the lockFlag was set lock the file
            if (!errToSet && lockFlag)
            {
                file->currentlyLockedBy.id = ownerId.id;
            }

            // set who has opened the file as entry of the openedBy list of the file
            if (!errToSet)
            {
                oid = malloc(sizeof(*oid));
                IS_NULL_DO(oid, { errToSet = E_FS_MALLOC; })
            }

            if (!errToSet)
            {
                oid->id = ownerId.id;
                List_insertHead(file->openedBy, oid, &errToSet);
                TO_GENERAL_ERROR(errToSet);
            }

            if (!errToSet)
            {
                insertIntoList = 1;
            }

            if (!errToSet && isAlreadyThereFile)
            {
                // this functions invalidates the ability to write on the file if it was already present
                file->ownerCanWrite.id = 0;
            }

            // in case of errors, cancel the insertion in the file's openedBy list
            if (errToSet)
            {
                if (oid && insertIntoList)
                {
                    List_extractHead(file->openedBy, NULL);
                }
                oid ? free(oid) : (void)NULL;
            }
        }

        // get the file's mutex lock
        if (!hasMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND && file)
        {
            hasMutex = 1;
            NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                        {
                            errToSet = E_FS_MUTEX;
                            hasMutex = 0;
                        })
        }

        // end of work
        if (!errToSet || errToSet == E_FS_FILE_IS_LOCKED || errToSet == E_FS_MALLOC)
        {
            file->activeWriters--;
        }

        if (hasMutex)
        {

            NON_ZERO_DO(pthread_cond_signal(&file->go), {
                errToSet = E_FS_COND;
            })

            hasMutex = 0;
            NON_ZERO_DO(pthread_mutex_unlock(&file->mutex),
                        {
                            errToSet = E_FS_MUTEX;
                            hasMutex = 1;
                        })
        }
    }

    // in case of initial errors, always release the file-system mutex
    if (hasFSMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND)
    {
        hasFSMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 1;
                    })
    }

    SET_ERROR;
    return evictedFile;
}

// Read a file
//
// Notes:
// - the client must have opened the file before
// - the path argument is owned by the caller
ResultFile FileSystem_readFile(FileSystem fs, char *path, OwnerId ownerId, int *error)
{
    int errToSet = 0;
    int hasFSMutex = 0; // overall mutex
    int hasMutex = 0;
    int hasOrdering = 0;
    int activeReadersUpdated = 0;

    File file = NULL;
    ResultFile resultFile = NULL;

    // check arguments
    if (fs == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    if (path == NULL)
    {
        errToSet = E_FS_INVALID_ARGUMENTS;
    }

    // acquire overall mutex
    if (!errToSet)
    {
        hasFSMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 0;
                    })
    }

    // find the file
    if (!errToSet)
    {
        file = icl_hash_find(fs->filesDict, path);
        IS_NULL_DO(file, { errToSet = E_FS_FILE_NOT_FOUND; })
    }

    // in case of LRU policy, place the file on top of the list
    if (!errToSet && fs->replacementPolicy == FS_REPLACEMENT_LRU)
    {
        struct File temp;
        temp.path = file->path;
        List_insertHead(fs->filesList, List_searchExtract(fs->filesList, filePathComparator, &temp, NULL), &errToSet);
        TO_GENERAL_ERROR(errToSet);
    }

    // acquire the file's mutexes
    if (!errToSet)
    {
        hasOrdering = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->ordering),
                    {
                        errToSet = E_FS_MUTEX;
                        hasOrdering = 0;
                    })
    }

    if (!errToSet)
    {
        hasMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 0;
                    })
    }

    // release the file-system mutex
    if (hasFSMutex)
    {
        hasFSMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 1;
                    })
    }

    if (!errToSet)
    {
        while (file->activeWriters > 0 && !errToSet)
        {
            NON_ZERO_DO(pthread_cond_wait(&file->go, &file->mutex), {
                errToSet = E_FS_COND;
            })
        }
    }

    // this function acts as a reader
    if (!errToSet)
    {
        file->activeReaders++;
        activeReadersUpdated = 1;
    }

    // release the file's mutexes
    if (hasOrdering)
    {
        hasOrdering = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->ordering),
                    {
                        errToSet = E_FS_MUTEX;
                        hasOrdering = 1;
                    })
    }

    if (hasMutex)
    {
        hasMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 1;
                    })
    }

    // fail if the file was locked by someone else
    if (!errToSet)
    {
        if (file->currentlyLockedBy.id != 0 && file->currentlyLockedBy.id != ownerId.id)
        {
            errToSet = E_FS_FILE_IS_LOCKED;
        }
    }

    // fail if the file was not opened by the requestor
    if (!errToSet)
    {
        int alreadyOpened = List_search(file->openedBy, List_getComparator(file->openedBy, NULL), &ownerId, &errToSet);
        TO_GENERAL_ERROR(errToSet);
        if (!errToSet && !alreadyOpened)
        {
            errToSet = E_FS_FILE_NOT_OPENED;
        }
    }

    // create the result
    if (!errToSet)
    {
        // this functions invalidates the ability to write on the file
        file->ownerCanWrite.id = 0;

        resultFile = malloc(sizeof(*resultFile));
        IS_NULL_DO(resultFile, { errToSet = E_FS_MALLOC; })
    }

    // fulfill the result
    if (!errToSet)
    {
        resultFile->size = file->size;
        resultFile->data = malloc(sizeof(*resultFile->data) * file->size);
        IS_NULL_DO(resultFile->data, { errToSet = E_FS_MALLOC; })
    }

    if (!errToSet)
    {
        memcpy(resultFile->data, file->data, file->size);
    }

    if (!errToSet)
    {
        resultFile->path = malloc(sizeof(*resultFile->path) * (strlen(file->path) + 1));
        IS_NULL_DO(resultFile->path, { errToSet = E_FS_MALLOC; })
    }

    if (!errToSet)
    {
        memcpy(resultFile->path, file->path, strlen(file->path) + 1);
    }

    // acquire the file's lock
    if (!errToSet)
    {
        hasMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 0;
                    })
    }

    // work is done
    if (errToSet != E_FS_MUTEX && errToSet != E_FS_COND)
    {
        if (activeReadersUpdated)
        {
            file->activeReaders--;

            if (file->activeReaders == 0)
            {
                NON_ZERO_DO(pthread_cond_signal(&file->go), {
                    errToSet = E_FS_COND;
                })
            };
        }

        if (hasMutex)
        {
            hasMutex = 0;
            NON_ZERO_DO(pthread_mutex_unlock(&file->mutex),
                        {
                            errToSet = E_FS_MUTEX;
                            hasMutex = 1;
                        })
        }
    }

    // in case of errors, destroy the result
    if (errToSet && resultFile)
    {
        resultFile->data ? free(resultFile->data) : (void)NULL;
        resultFile->path ? free(resultFile->path) : (void)NULL;
        free(resultFile);
        resultFile = NULL;
    }

    SET_ERROR;
    return resultFile;
}

// Read at most N files
List_T FileSystem_readNFile(FileSystem fs, OwnerId ownerId, int N, int *error)
{
    int errToSet = 0;
    int hasFSMutex = 0; // file-system mutex

    int counter = 0;
    int filesListLen = 0;
    List_T resultFiles = NULL;

    // check arguments
    if (fs == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    // acquire overall mutex
    if (!errToSet)
    {
        hasFSMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 0;
                    })
    }

    // set the variable containing how many files are in the list
    if (!errToSet)
    {
        filesListLen = List_length(fs->filesList, &errToSet);
        TO_GENERAL_ERROR(errToSet);
    }

    // how many files have to be read
    if (!errToSet)
    {
        // the filesListLen is an upperbound
        counter = (N >= filesListLen || N <= 0) ? filesListLen : N;
    }

    // create the list where to put the results
    if (!errToSet)
    {
        resultFiles = List_create(NULL, NULL, fileResultDeallocator, &errToSet);
        TO_GENERAL_ERROR(errToSet);
    }

    // read counter files
    if (!errToSet)
    {
        struct ReadNFileSetResultFilesContext ctx;
        ctx.resultFiles = resultFiles;
        ctx.counter = counter;
        ctx.ownerId = ownerId;
        List_forEachWithContext(fs->filesList, readNFiles, &ctx, &errToSet);
        TO_GENERAL_ERROR(errToSet);
    }

    // if the LRU policy was chosen, put each read file on top of the list
    if (!errToSet && fs->replacementPolicy == FS_REPLACEMENT_LRU)
    {
        struct ReadNFileLRUContext ctx;
        ctx.files = fs->filesList;
        List_forEachWithContext(resultFiles, moveFilesLRU, &ctx, &errToSet);
        TO_GENERAL_ERROR(errToSet);
    }

    // in case of errors, destroy the resultFiles list that may be inconsistent
    if (errToSet)
    {
        resultFiles ? List_free(&resultFiles, 1, NULL) : (void)NULL;
        resultFiles = NULL;
    }

    // release the overall mutex
    if (hasFSMutex)
    {
        hasFSMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 1;
                    })
    }

    SET_ERROR;
    return resultFiles;
}

// Append/Write text to a file
//
// Notes:
// - the path argument is owned by the caller
// - the content argument is owned by the caller, so it will be cloned
// - if the write flag is set to 0 the function behaves as append, otherwise it behaves as write
List_T FileSystem_appendToFile(FileSystem fs, char *path, char *content, size_t contentSize, OwnerId ownerId, int write, int *error)
{

    int errToSet = 0;
    int hasFSMutex = 0;  // overall (file-system) mutex
    int hasMutex = 0;    // of the file
    int hasOrdering = 0; // of the file
    int activeWritersUpdated = 0;

    File file = NULL;
    List_T evictedFiles = NULL;

    // check arguments
    if (fs == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    if (!errToSet && (path == NULL || content == NULL || contentSize == 0))
    {
        errToSet = E_FS_INVALID_ARGUMENTS;
    }

    // create a list to insert all the evicted files
    if (!errToSet)
    {
        evictedFiles = List_create(NULL, NULL, fileResultDeallocator, &errToSet);
        TO_GENERAL_ERROR(errToSet);
    }

    // acquire the file-system mutex
    if (!errToSet)
    {
        hasFSMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 0;
                    })
    }

    // is there enough overall space for the new content?
    if (!errToSet)
    {
        if (contentSize >= fs->maxStorageSize)
        {
            errToSet = E_FS_EXCEEDED_SIZE;
        }
    }

    // find the desired file
    if (!errToSet)
    {
        file = icl_hash_find(fs->filesDict, path);
        IS_NULL_DO(file, { errToSet = E_FS_FILE_NOT_FOUND; })
    }

    // acquire its locks
    if (!errToSet)
    {
        hasOrdering = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->ordering),
                    {
                        errToSet = E_FS_MUTEX;
                        hasOrdering = 0;
                    })
    }

    if (!errToSet)
    {
        hasMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 0;
                    })
    }

    if (!errToSet)
    {
        while (!errToSet && (file->activeReaders > 0 || file->activeWriters > 0))
        {
            NON_ZERO_DO(pthread_cond_wait(&file->go, &file->mutex), {
                errToSet = E_FS_COND;
            })
        }
    }

    // this function acts as a writer
    if (!errToSet)
    {
        file->activeWriters++;
        activeWritersUpdated = 1;
    }

    // release the file's locks
    if (hasOrdering)
    {
        hasOrdering = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->ordering),
                    {
                        errToSet = E_FS_MUTEX;
                        hasOrdering = 1;
                    })
    }

    if (hasMutex)
    {
        hasMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 1;
                    })
    }

    // if no error has occurred
    if (!errToSet)
    {

        // if I added the content to the file, would I run out of space?
        if (file->size + contentSize >= fs->maxStorageSize)
        {
            errToSet = E_FS_EXCEEDED_SIZE;
        }

        // check if the file was previously locked by someone else
        if (!errToSet && file->currentlyLockedBy.id != 0 && file->currentlyLockedBy.id != ownerId.id)
        {
            errToSet = E_FS_FILE_IS_LOCKED;
        }

        // if the function behave as write, it can write only if
        // the previous operation was openFile(path, O_CREATE| O_LOCK)
        if (!errToSet && write && file->ownerCanWrite.id != ownerId.id)
        {
            errToSet = E_FS_FILE_NO_WRITE;
        }

        // check if the client had previously opened the file
        if (!errToSet)
        {
            OwnerId _ownerId;
            _ownerId.id = ownerId.id;

            if (!List_search(file->openedBy, ownerIdComparator, &_ownerId, NULL))
            {
                errToSet = E_FS_FILE_NOT_OPENED;
            }
        }

        // evict files until there is enough space
        while (!errToSet && ((fs->currentStorageSize + contentSize) >= fs->maxStorageSize) && fs->currentNumOfFiles != 0)
        {
            ResultFile evicted = FileSystem_evict(fs, file->path, &errToSet);
            if (!errToSet)
            {
                List_insertHead(evictedFiles, evicted, &errToSet);
                TO_GENERAL_ERROR(errToSet);
            }
        }

        // increase the file's space
        if (!errToSet)
        {
            NON_ZERO_DO(realloca(&file->data, file->size + contentSize), {
                errToSet = E_FS_MALLOC;
            })
        }

        // clone the new content and update the file's info
        if (!errToSet)
        {
            memcpy(file->data + file->size, content, contentSize);
            file->size += contentSize;
            fs->currentStorageSize += contentSize;
            fs->maxByteOfStorageUsed = fs->currentStorageSize > fs->maxByteOfStorageUsed ? fs->currentStorageSize : fs->maxByteOfStorageUsed;

            // this functions invalidates the ability to write on the file
            file->ownerCanWrite.id = 0;
        }

        // if the LRU replacement policy was enabled, put this file on top of the list
        if (!errToSet && fs->replacementPolicy == FS_REPLACEMENT_LRU)
        {
            struct File temp;
            temp.path = file->path;
            List_insertHead(fs->filesList, List_searchExtract(fs->filesList, filePathComparator, &temp, NULL), &errToSet);
            TO_GENERAL_ERROR(errToSet);
        }

        // release the overall lock
        if (hasFSMutex)
        {
            hasFSMutex = 0;
            NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex),
                        {
                            errToSet = E_FS_MUTEX;
                            hasFSMutex = 1;
                        })
        }
    }

    // acquire again the file's mutex
    if (!hasMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND && file)
    {
        hasMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 0;
                    })
    }

    // end of work
    if (errToSet != E_FS_MUTEX && errToSet != E_FS_COND && activeWritersUpdated)
    {
        file->activeWriters--;
    }

    if (hasMutex)
    {
        NON_ZERO_DO(pthread_cond_signal(&file->go), {
            errToSet = E_FS_COND;
        })

        hasMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 1;
                    })
    }

    // if an error has occurred, the evictedFiles list may be inconsistent
    // so it will be erased
    if (errToSet)
    {
        evictedFiles ? List_free(&evictedFiles, 1, NULL) : (void)NULL;
    }

    // always release the overall lock
    if (hasFSMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND)
    {
        hasFSMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 1;
                    })
    }

    SET_ERROR;
    return evictedFiles;
}

// Logically lock a file
//
// Note: the path argument is owned by the caller
void FileSystem_lockFile(FileSystem fs, char *path, OwnerId ownerId, int *error)
{
    int errToSet = 0;
    int hasFSMutex = 0; // file-system mutex
    int hasMutex = 0;
    int hasOrdering = 0;
    int activeWritersUpdated = 0;
    File file = NULL;

    // arguments check
    if (fs == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    if (!errToSet && (path == NULL))
    {
        errToSet = E_FS_INVALID_ARGUMENTS;
    }

    // acquire the overall mutex
    if (!errToSet)
    {
        hasFSMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 0;
                    })
    }

    // find the file inside the file-system
    if (!errToSet)
    {
        file = icl_hash_find(fs->filesDict, path);
        IS_NULL_DO(file, { errToSet = E_FS_FILE_NOT_FOUND; })
    }

    // acquire file's locks
    if (!errToSet)
    {
        hasOrdering = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->ordering),
                    {
                        errToSet = E_FS_MUTEX;
                        hasOrdering = 0;
                    })
    }

    if (!errToSet)
    {
        hasMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 0;
                    })
    }

    // in case of LRU policy, put the file on top of the list
    if (!errToSet && fs->replacementPolicy == FS_REPLACEMENT_LRU)
    {
        struct File temp;
        temp.path = file->path;
        List_insertHead(fs->filesList, List_searchExtract(fs->filesList, filePathComparator, &temp, NULL), &errToSet);
        TO_GENERAL_ERROR(errToSet);
    }

    // unlock the overall mutex
    if (!errToSet)
    {
        hasFSMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 1;
                    })
    }

    if (!errToSet)
    {
        while (!errToSet && (file->activeReaders > 0 || file->activeWriters > 0))
        {
            NON_ZERO_DO(pthread_cond_wait(&file->go, &file->mutex), {
                errToSet = E_FS_COND;
            })
        }
    }

    // this function acts as a writer
    if (!errToSet)
    {
        file->activeWriters++;
        activeWritersUpdated = 1;
    }

    // release file's locks
    if (hasOrdering)
    {
        hasOrdering = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->ordering),
                    {
                        errToSet = E_FS_MUTEX;
                        hasOrdering = 1;
                    })
    }

    if (hasMutex)
    {
        hasMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 1;
                    })
    }

    // if no error has occurred, try to lock the file
    if (!errToSet)
    {
        // this functions invalidates the ability to write on the file
        file->ownerCanWrite.id = 0;

        // if the file was already locked by someone else, enter the waiting list (file->waitingLockers)
        if (file->currentlyLockedBy.id != ownerId.id && file->currentlyLockedBy.id != 0)
        {

            OwnerId *oid = malloc(sizeof(*oid));
            if (oid)
            {
                oid->id = ownerId.id;
                int present = List_search(file->waitingLockers, ownerIdComparator, oid, &errToSet);
                TO_GENERAL_ERROR(errToSet);

                if (!errToSet && !present)
                {
                    List_insertTail(file->waitingLockers, oid, &errToSet);
                    TO_GENERAL_ERROR(errToSet);
                }

                if (!errToSet && present)
                {
                    errToSet = E_FS_FILE_ALREADY_LOCKED;
                }

                if (errToSet)
                {
                    free(oid);
                }
                else
                {
                    // an error is returned anyway as feedback
                    errToSet = E_FS_FILE_IS_LOCKED;
                }
            }
            else
            {
                errToSet = E_FS_MALLOC;
            }
        }

        // if the file was already locked by the requestor an error is returned anyway as feedback
        if (!errToSet && file->currentlyLockedBy.id == ownerId.id)
        {
            errToSet = E_FS_FILE_ALREADY_LOCKED;
        }

        // if the file was not locked by anyone, lock it
        if (!errToSet && file->currentlyLockedBy.id == 0)
        {
            file->currentlyLockedBy.id = ownerId.id;
        }
    }

    // acquire the file's mutex lock
    if (!hasMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND && file)
    {
        hasMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 0;
                    })
    }

    // work is done
    if (errToSet != E_FS_MUTEX && errToSet != E_FS_COND && activeWritersUpdated)
    {
        file->activeWriters--;
    }

    if (hasMutex)
    {
        NON_ZERO_DO(pthread_cond_signal(&file->go), {
            errToSet = E_FS_COND;
        })

        hasMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 1;
                    })
    }

    // always release the overall mutex
    if (hasFSMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND)
    {
        hasFSMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 1;
                    })
    }

    SET_ERROR;
}

// Logically unlock a file
//
// Notes:
// - the path argument is owned by the caller
// - if an OwnerId is returned, it corresponds to the waiting client that has gained the lock on the file
OwnerId *FileSystem_unlockFile(FileSystem fs, char *path, OwnerId ownerId, int *error)
{
    int errToSet = 0;
    int hasFSMutex = 0; // file-system mutex
    int hasMutex = 0;
    int hasOrdering = 0;
    int activeWritersUpdated = 0;

    OwnerId *oidToRet = NULL;
    File file = NULL;

    // arguments check
    if (fs == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    if (!errToSet && (path == NULL))
    {
        errToSet = E_FS_INVALID_ARGUMENTS;
    }

    // acquire the overall mutex
    if (!errToSet)
    {
        hasFSMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 0;
                    })
    }

    // find the file
    if (!errToSet)
    {
        file = icl_hash_find(fs->filesDict, path);
        IS_NULL_DO(file, { errToSet = E_FS_FILE_NOT_FOUND; })
    }

    // acquire the file's mutexes
    if (!errToSet)
    {
        hasOrdering = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->ordering),
                    {
                        errToSet = E_FS_MUTEX;
                        hasOrdering = 0;
                    })
    }

    if (!errToSet)
    {
        hasMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 0;
                    })
    }

    // in case of LRU policy, put the file on top of the list
    if (!errToSet && fs->replacementPolicy == FS_REPLACEMENT_LRU)
    {
        struct File temp;
        temp.path = file->path;
        List_insertHead(fs->filesList, List_searchExtract(fs->filesList, filePathComparator, &temp, NULL), &errToSet);
        TO_GENERAL_ERROR(errToSet);
    }

    // release the overall mutex
    if (!errToSet)
    {
        hasFSMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 1;
                    })
    }

    if (!errToSet)
    {
        while (!errToSet && (file->activeReaders > 0 || file->activeWriters > 0))
        {
            NON_ZERO_DO(pthread_cond_wait(&file->go, &file->mutex), {
                errToSet = E_FS_COND;
            })
        }
    }

    // this function acts as a writer
    if (!errToSet)
    {
        file->activeWriters++;
        activeWritersUpdated = 1;
    }

    // release the file's mutexes
    if (hasOrdering)
    {
        hasOrdering = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->ordering),
                    {
                        errToSet = E_FS_MUTEX;
                        hasOrdering = 1;
                    })
    }

    if (hasMutex)
    {
        hasMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 1;
                    })
    }

    // if no error has occurred
    if (!errToSet)
    {
        // this functions invalidates the ability to write on the file
        file->ownerCanWrite.id = 0;

        // fail if the file was locked by someone else
        if (!errToSet && file->currentlyLockedBy.id != ownerId.id)
        {
            errToSet = E_FS_FILE_IS_LOCKED;
        }

        // fail if the file was not locked bt anyone
        if (!errToSet && file->currentlyLockedBy.id == 0)
        {
            errToSet = E_FS_FILE_NOT_LOCKED;
        }

        // if the requestor has locked the file
        if (!errToSet && file->currentlyLockedBy.id == ownerId.id)
        {
            int listLength = List_length(file->waitingLockers, &errToSet);
            TO_GENERAL_ERROR(errToSet);
            int lockedOwnerToSet = 0;

            // if someone else was waiting to set the lock on the file...
            if (!errToSet && listLength >= 1)
            {
                // ...extract it from the waiting list
                oidToRet = List_extractHead(file->waitingLockers, &errToSet);
                TO_GENERAL_ERROR(errToSet);
                if (!errToSet)
                {
                    lockedOwnerToSet = oidToRet->id;
                }
                else
                {
                    oidToRet = NULL;
                }
            }

            // set the extracted id as the current logical locker or reset the latter one
            file->currentlyLockedBy.id = lockedOwnerToSet;
        }
    }

    // acquire the mutex lock of the file
    if (!hasMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND && file)
    {
        hasMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 0;
                    })
    }

    // work is done
    if (errToSet != E_FS_MUTEX && errToSet != E_FS_COND && activeWritersUpdated)
    {
        file->activeWriters--;
    }

    // release and signal
    if (hasMutex)
    {
        NON_ZERO_DO(pthread_cond_signal(&file->go), {
            errToSet = E_FS_COND;
        })

        hasMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 1;
                    })
    }

    // always release the overall mutex
    if (hasFSMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND)
    {
        hasFSMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 1;
                    })
    }

    SET_ERROR;
    return oidToRet;
}

// Close a file
//
// Notes:
// - the path argument is owned by the caller
void FileSystem_closeFile(FileSystem fs, char *path, OwnerId ownerId, int *error)
{
    int errToSet = 0;
    int hasFSMutex = 0; // overall mutex
    int hasMutex = 0;
    int hasOrdering = 0;
    int activeWritersUpdated = 0;
    File file = NULL;

    // arguments check
    if (fs == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    if (!errToSet && (path == NULL))
    {
        errToSet = E_FS_INVALID_ARGUMENTS;
    }

    // acquire the file-system mutex
    if (!errToSet)
    {
        hasFSMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 0;
                    })
    }

    // find the file
    if (!errToSet)
    {
        file = icl_hash_find(fs->filesDict, path);
        IS_NULL_DO(file, { errToSet = E_FS_FILE_NOT_FOUND; })
    }

    // acquire the file's mutexes
    if (!errToSet)
    {
        hasOrdering = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->ordering),
                    {
                        errToSet = E_FS_MUTEX;
                        hasOrdering = 0;
                    })
    }

    if (!errToSet)
    {
        hasMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 0;
                    })
    }

    // in case of LRU policy, put the file on top of the list
    if (!errToSet && fs->replacementPolicy == FS_REPLACEMENT_LRU)
    {
        struct File temp;
        temp.path = file->path;
        List_insertHead(fs->filesList, List_searchExtract(fs->filesList, filePathComparator, &temp, NULL), &errToSet);
        TO_GENERAL_ERROR(errToSet);
    }

    // release the overall mutex
    if (!errToSet)
    {
        hasFSMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 1;
                    })
    }

    if (!errToSet)
    {
        while (!errToSet && (file->activeReaders > 0 || file->activeWriters > 0))
        {
            NON_ZERO_DO(pthread_cond_wait(&file->go, &file->mutex), {
                errToSet = E_FS_COND;
            })
        }
    }

    // this function acts as a writer
    if (!errToSet)
    {
        file->activeWriters++;
        activeWritersUpdated = 1;
    }

    // release the file's mutexes
    if (hasOrdering)
    {
        hasOrdering = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->ordering),
                    {
                        errToSet = E_FS_MUTEX;
                        hasOrdering = 1;
                    })
    }

    if (hasMutex)
    {
        hasMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 1;
                    })
    }

    // if no error has occurred
    if (!errToSet)
    {
        OwnerId _ownerId;
        _ownerId.id = ownerId.id;

        // To close a file it has to be opened before
        if (!List_search(file->openedBy, ownerIdComparator, &_ownerId, NULL))
        {
            errToSet = E_FS_FILE_NOT_OPENED;
        }

        if (!errToSet)
        {
            OwnerId *ext = List_searchExtract(file->openedBy, ownerIdComparator, &_ownerId, &errToSet);
            TO_GENERAL_ERROR(errToSet);
            if (!errToSet)
            {
                //the "has opened the file?" control was passed, so 'ext' won't be NULL
                free(ext);
            }
        }
    }

    // acquire again the file's mutex
    if (!hasMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND && file)
    {
        hasMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 0;
                    })
    }

    // work is done
    if (errToSet != E_FS_MUTEX && errToSet != E_FS_COND && activeWritersUpdated)
    {
        file->activeWriters--;
    }

    // release the file's mutex and signal
    if (hasMutex)
    {
        NON_ZERO_DO(pthread_cond_signal(&file->go), {
            errToSet = E_FS_COND;
        })

        hasMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 1;
                    })
    }

    // always release the overall mutex
    if (hasFSMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND)
    {
        hasFSMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 1;
                    })
    }

    SET_ERROR;
}

// Remove a file from the file-system
//
// Note: never release the file-system mutex because we don't want another
// client waiting on a lock that is going to be destroyed
void FileSystem_removeFile(FileSystem fs, char *path, OwnerId ownerId, int *error)
{
    int errToSet = 0;
    int hasFSMutex = 1; // overall mutex
    int hasMutex = 0;
    int hasOrdering = 0;

    File file = NULL;

    // arguments check
    if (fs == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    if (!errToSet && (path == NULL))
    {
        errToSet = E_FS_INVALID_ARGUMENTS;
    }

    // acquire the file-system mutex
    if (!errToSet)
    {
        NON_ZERO_DO(pthread_mutex_lock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 0;
                    })
    }

    // find the file
    if (!errToSet)
    {
        file = icl_hash_find(fs->filesDict, path);
        IS_NULL_DO(file, { errToSet = E_FS_FILE_NOT_FOUND; })
    }

    // acquire the file's locks
    if (!errToSet)
    {
        hasOrdering = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->ordering),
                    {
                        errToSet = E_FS_MUTEX;
                        hasOrdering = 0;
                    })
    }

    if (!errToSet)
    {
        hasMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 0;
                    })
    }

    while (!errToSet && (file->activeReaders > 0 || file->activeWriters > 0))
    {
        NON_ZERO_DO(pthread_cond_wait(&file->go, &file->mutex), {
            errToSet = E_FS_COND;
        })
    }

    // release the file's mutexes
    if (hasOrdering)
    {
        hasOrdering = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->ordering),
                    {
                        errToSet = E_FS_MUTEX;
                        hasOrdering = 1;
                    })
    }

    if (hasMutex)
    {
        hasMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 1;
                    })
    }

    // fail if the file was locked by someone else
    if (!errToSet && file->currentlyLockedBy.id != 0 && file->currentlyLockedBy.id != ownerId.id)
    {
        errToSet = E_FS_FILE_IS_LOCKED;
    }

    // fail if the file was not locked by the requestor
    if (!errToSet && file->currentlyLockedBy.id == 0)
    {
        errToSet = E_FS_FILE_NOT_LOCKED;
    }

    // if no error has occurred, attempt to remove the file from the file-system's list
    if (!errToSet && file->currentlyLockedBy.id == ownerId.id)
    {
        struct File temp;
        temp.path = path;
        file = List_searchExtract(fs->filesList, filePathComparator, &temp, &errToSet);
        TO_GENERAL_ERROR(errToSet);
    }

    // if no error has occurred, remove the file from the dict as well
    if (!errToSet)
    {
        icl_hash_delete(fs->filesDict, file->path, NULL, NULL);

        // then update the file-system
        fs->currentNumOfFiles--;
        fs->currentStorageSize -= file->size;

        // free the file
        free(file->data);
        free(file->path);
        List_free(&file->openedBy, 1, NULL);
        List_free(&file->waitingLockers, 1, NULL);
        free(file);
    }

    // release the file-system lock
    if (hasFSMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND)
    {
        hasFSMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 1;
                    })
    }

    SET_ERROR;
}

// Remove the presence of a client (ownerId) from all the structures inside the file-system
//
// Notes:
// - never release the file-system mutex because we visit and may modify lot of files'structures
// - return a list of OwnerIds that have gained a logical file's lock
List_T FileSystem_evictClient(FileSystem fs, OwnerId ownerId, int *error)
{

    int errToSet = 0;
    int hasFSMutex = 0; // file-system (overall) mutex
    struct EvictClientContext ctx;
    List_T oidsHaveLocks = NULL;

    // argument check
    if (fs == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    // acquire the overall mutex
    if (!errToSet)
    {
        hasFSMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 0;
                    })
    }

    // initialize the context
    if (!errToSet)
    {
        ctx.oid.id = ownerId.id;
        // a list of OwnerIds does not need a custom Deallocator function
        oidsHaveLocks = List_create(NULL, NULL, NULL, &errToSet);
        TO_GENERAL_ERROR(errToSet);
        ctx.oidsHaveLocks = oidsHaveLocks;
    }

    if (!errToSet)
    {
        // evict the client from every-where in the file-system
        List_forEachWithContext(fs->filesList, evictClientInternal, &ctx, &errToSet);
        TO_GENERAL_ERROR(errToSet);
    }

    // if a list error has occurred, map it into a file-system error and destroy the oidsHaveLocks list that may be inconsistent
    if (errToSet)
    {
        List_free(&oidsHaveLocks, 1, &errToSet);
        TO_GENERAL_ERROR(errToSet);
    }

    // release the file-system mutex
    if (hasFSMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND)
    {
        hasFSMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 1;
                    })
    }

    SET_ERROR;
    return oidsHaveLocks;
}

// Free a ResultFile structure
void ResultFile_free(ResultFile *rfPtr, int *error)
{
    int errToSet = 0;

    // arguments check
    IS_NULL_DO(rfPtr, errToSet = E_FS_RESULTFILE_IS_NULL);

    if (!errToSet)
    {
        IS_NULL_DO(*rfPtr, errToSet = E_FS_RESULTFILE_IS_NULL);
    }

    // free what has to be freed
    if (!errToSet)
    {
        free((*rfPtr)->data);
        free((*rfPtr)->path);
        free(*rfPtr);
        *rfPtr = NULL;
    }

    SET_ERROR
}

// Get the total size in bytes of the file-system
size_t ResultFile_getCurrentSizeInByte(FileSystem fs, int *error)
{
    int errToSet = 0;
    int hasFSMutex = 1; // overall mutex
    int size = 0;

    if (fs == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    if (!errToSet)
    {
        NON_ZERO_DO(pthread_mutex_lock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 0;
                    })
    }

    if (!errToSet)
    {
        size = fs->currentStorageSize;
    }

    if (hasFSMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND)
    {
        hasFSMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 1;
                    })
    }

    SET_ERROR;
    return size;
}

// Get the total number of files stored inside the file-system
size_t ResultFile_getCurrentNumOfFiles(FileSystem fs, int *error)
{
    int errToSet = 0;
    int hasFSMutex = 1; // overall mutex
    int numOfFiles = 0;

    if (fs == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    if (!errToSet)
    {
        NON_ZERO_DO(pthread_mutex_lock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 0;
                    })
    }

    if (!errToSet)
    {
        numOfFiles = fs->currentNumOfFiles;
    }

    if (hasFSMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND)
    {
        hasFSMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 1;
                    })
    }

    SET_ERROR;
    return numOfFiles;
}

// Get all the files' path stored inside the file-system
Paths ResultFile_getStoredFilesPaths(FileSystem fs, int *error)
{

    int errToSet = 0;
    int hasFSMutex = 1; // overall mutex
    Paths ps = NULL;

    if (fs == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    if (!errToSet)
    {
        ps = malloc(sizeof(*ps));
        IS_NULL_DO_ELSE(ps, errToSet = E_FS_MALLOC,
                        {
                            ps->index = 0;
                            ps->paths = NULL;
                        })
    }

    if (!errToSet)
    {
        NON_ZERO_DO(pthread_mutex_lock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 0;
                    })
    }

    if (!errToSet)
    {
        ps->paths = calloc(fs->currentNumOfFiles + 1, sizeof(*ps->paths));
        IS_NULL_DO(ps->paths, errToSet = E_FS_MALLOC);
    }

    if (!errToSet)
    {
        List_forEachWithContext(fs->filesList, extractPaths, ps, &errToSet);
        TO_GENERAL_ERROR(errToSet)
    }

    if (errToSet)
    {
        ps ? free(ps->paths) : (void)NULL;
        free(ps);
        ps = NULL;
    }

    if (hasFSMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND)
    {
        hasFSMutex = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 1;
                    })
    }

    SET_ERROR;
    return ps;
}

const char *filesystem_error_messages[] = {
    "filesystem internal malloc error",
    "filesystem is null",
    "filesystem general error",
    "filesystem internal mutex error",
    "filesystem internal cond error",
    "filesystem unknown replacement policy",
    "filesystem too many files",
    "filesystem size overflow",
    "filesystem invalid arguments",
    "filesystem file already present",
    "filesystem file not found",
    "filesystem file is locked",
    "filesystem file not opened",
    "filesystem file write is forbidden, use append instead",
    "filesystem file is not locked",
    "filesystem file already locked",
    "filesystem result-file is null",
    "filesystem eviction failed",
};

const char *FileSystem_getErrorMessage(int errorCode)
{
    return filesystem_error_messages[errorCode - 11];
}

// ------------------------------------------------------------------------------------------------------------------
// DEBUG FUNCTIONS

// ACHTUNG: only for debug purposes, it is not thread safe (meant to be used in a single threaded environment)
// nor production ready (errors are discarded, no input checks)
void printOwnerIdInfo_DEBUG(void *rawOwnerId, int *_)
{
    OwnerId *oid = rawOwnerId;
    puts("@@@@@@@@@@@@@@@@@@@");
    printf("OwnerId is %d\n", oid->id);
    puts("@@@@@@@@@@@@@@@@@@@");
}

// ACHTUNG: only for debug purposes, it is not thread safe (meant to be used in a single threaded environment)
// nor production ready (errors are discarded, no input checks)
void printFileInfo_DEBUG(void *rawFile, int *_)
{
    File file = rawFile;
    puts("##############################");
    printf("File Path is: %s\n", file->path);
    printf("File Data is: %s\n", file->data);
    printf("File Size is: %d\n", file->size);
    printf("file->currentlyLockedBy is: %d\n", file->currentlyLockedBy.id);
    printf("file->ownerCanWrite is: %d\n", file->ownerCanWrite.id);
    puts("Openers Queue:");
    List_forEach(file->openedBy, printOwnerIdInfo_DEBUG, NULL);
    puts("\nLock Waiting Queue:");
    List_forEach(file->waitingLockers, printOwnerIdInfo_DEBUG, NULL);
    puts("##############################");
}

// ACHTUNG: only for debug purposes, it is not thread safe (meant to be used in a single threaded environment)
// nor production ready (errors are discarded, no input checks)
void FileSystem_printAll_DEBUG(FileSystem fs)
{
    puts("\n\n**************************************************");
    puts("**************************************************");
    puts("**************************************************");
    puts("FileSystem DEBUG INFO");
    printf("REPLACEMENT POLICY IS %d\n", fs->replacementPolicy);
    printf("MAX NUM OF FILES IS %d\n", fs->maxNumOfFiles);
    printf("MAX FILE-SYSTEM SIZE IN BYTE IS %d\n", fs->maxStorageSize);
    printf("CURRENT NUM OF FILES IS %d\n", fs->currentNumOfFiles);
    printf("CURRENT FILE-SYSTEM SIZE IN BYTE IS IS %d\n", fs->currentStorageSize);
    puts("\nFILES INFO:\n");
    List_forEach(fs->filesList, printFileInfo_DEBUG, NULL);
    puts("**************************************************");
    puts("**************************************************");
    puts("**************************************************\n");
}