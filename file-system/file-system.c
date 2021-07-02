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

void fileDeallocator(void *rawFile)
{
    File file = rawFile;
    free(file->path);
    free(file->data);
    // free also the ownerIds inside the lists
    List_free(&(file->waitingLockers), 1, NULL);
    List_free(&(file->openedBy), 1, NULL);
}

void fileResultDeallocator(void *rawFile)
{
    ResultFile file = rawFile;
    free(file->path);
    free(file->data);
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

int filePathComparator(void *rawf1, void *rawf2)
{
    File f1 = rawf1;
    File f2 = rawf2;

    return strcmp(f1->path, f2->path) == 0;
}

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

void evictClientInternal(void *rawOwnerId, void *rawFile, int *error)
{
    // precondition: has overall mutex

    OwnerId *oid = rawOwnerId;
    OwnerId *oidToFree = NULL;
    File file = rawFile;
    int hasMutex = 0;
    int hasOrdering = 0;

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

    if (!(*error))
    {
        file->activeWriters++;
    }

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

    if (!(*error))
    {
        if (file->currentlyLockedBy.id == oid->id)
        {
            file->currentlyLockedBy.id == 0;
        }
        if (file->ownerCanWrite.id == oid->id)
        {
            // this functions invalidates the ability to write on the file
            file->ownerCanWrite.id = 0;
        }

        oidToFree = List_searchExtract(file->openedBy, ownerIdComparator, &oid, error);
    }

    if (!(*error))
    {
        (oidToFree ? free(oidToFree) : (void)NULL);
        oidToFree = NULL;
        oidToFree = List_searchExtract(file->waitingLockers, ownerIdComparator, &oid, error);
    }

    if (!(*error))
    {
        (oidToFree ? free(oidToFree) : (void)NULL);
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

    if ((*error) != E_FS_MUTEX && (*error) != E_FS_COND)
    {
        file->activeWriters--;
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
}

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

void readNFiles(void *rawCtx, void *rawFile, int *error)
{
    // assumption: it has overall mutex

    struct ReadNFileSetResultFilesContext *ctx = rawCtx;
    ResultFile resultFile = NULL;
    File file = rawFile;

    int hasMutex = 0;
    int hasOrdering = 0;
    // skip locked files
    int isLockedByOthers = 0;
    int activeReadersUpdated = 0;

    if (ctx->counter <= 0 || *error)
    {
        return;
    }
    else
    {
        ctx->counter--;
    }

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

    if (!(*error))
    {
        file->activeReaders++;
        activeReadersUpdated = 1;
    }

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

    // read

    if (!(*error))
    {
        isLockedByOthers = file->currentlyLockedBy.id != 0 && file->currentlyLockedBy.id != ctx->ownerId.id;
        if (isLockedByOthers)
        {
            ctx->counter++;
        }
    }

    if (!(*error) && !isLockedByOthers)
    {
        resultFile = malloc(sizeof(*resultFile));
        IS_NULL_DO(resultFile, { *error = E_FS_MALLOC; })
    }

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
        // this functions invalidates the ability to write on the file
        file->ownerCanWrite.id = 0;
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

    if (hasMutex)
    {
        hasMutex = 0;

        NON_ZERO_DO(pthread_mutex_unlock(&file->mutex),
                    {
                        *error = E_FS_MUTEX;
                        hasMutex = 1;
                    })
    }

    if (*error)
    {
        resultFile->data ? free(resultFile->data) : (void)NULL;
        resultFile->path ? free(resultFile->path) : (void)NULL;
        free(resultFile);
    }
}

void moveFilesLRU(void *rawCtx, void *rawResultFile, int *error)
{
    // assumption: it has overall mutex
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
        fs && (fs->filesDict) ? icl_hash_destroy(fs->filesDict, NULL, NULL) : 0;
        fs && (fs->filesList) ? List_free(&fs->filesList, 0, NULL) : (void)NULL;
        fs ? free(fs) : (void)NULL;
    }

    SET_ERROR;
    return fs;
}

// Note: a precondition is that this function must be called when all but one threads have died => no mutex is required
void FileSystem_delete(FileSystem *fsPtr, int *error)
{

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

        if (!errToSet)
        {
            // file: the file to evict
            file = List_extractTail(fs->filesList, &errToSet);
        }

        if (!errToSet)
        {
            // reinsert temp
            List_insertTail(fs->filesList, temp, &errToSet);
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
        }
    }

    if (!errToSet)
    {
        // remove the file from the dict as well
        icl_hash_delete(fs->filesDict, file->path, NULL, NULL);

        // acquire the ordering lock of the evicted file
        NON_ZERO_DO(pthread_mutex_lock(&file->ordering),
                    {
                        errToSet = E_FS_MUTEX;
                    })
    }

    if (!errToSet)
    {
        // acquire the mutex lock of the evicted file
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                    })
    }

    if (!errToSet)
    {
        // wait my turn
        while ((file->activeReaders > 0 || file->activeWriters > 0) && !errToSet)
        {
            NON_ZERO_DO(pthread_cond_wait(&file->go, &file->mutex), {
                errToSet = E_FS_COND;
            })
        }
    }

    if (!errToSet)
    {
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

        // free all the file's data that aren't going to be used again
        List_free(&file->openedBy, 1, NULL);
        List_free(&file->waitingLockers, 1, NULL);
        free(file);
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
            IS_NULL_DO(file->openedBy, { errToSet = E_FS_MALLOC; })
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
            IS_NULL_DO(file->waitingLockers, { errToSet = E_FS_MALLOC; })
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
            while ((file->activeReaders > 0 || file->activeWriters > 0) && !errToSet)
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
        if (!hasMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND)
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

ResultFile FileSystem_readFile(FileSystem fs, char *path, OwnerId ownerId, int *error)
{
    // the client must have opened the file before
    // is caller responsibility to free path
    int errToSet = 0;
    int hasFSMutex = 0;
    int hasMutex = 0;
    int hasOrdering = 0;
    int activeReadersUpdated = 0;

    File file = NULL;
    ResultFile resultFile = NULL;

    if (fs == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    if (path == NULL)
    {
        errToSet = E_FS_INVALID_ARGUMENTS;
    }

    if (!errToSet)
    {
        hasFSMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 0;
                    })
    }

    if (!errToSet)
    {
        file = icl_hash_find(fs->filesDict, path);
        IS_NULL_DO(file, { errToSet = E_FS_FILE_NOT_FOUND; })
    }

    if (!errToSet && fs->replacementPolicy == FS_REPLACEMENT_LRU)
    {
        struct File temp;
        temp.path = file->path;
        List_insertHead(fs->filesList, List_searchExtract(fs->filesList, filePathComparator, &temp, NULL), &errToSet);
    }

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

    if (!errToSet)
    {
        file->activeReaders++;
        activeReadersUpdated = 1;
    }

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
        if (file->currentlyLockedBy.id != 0 && file->currentlyLockedBy.id != ownerId.id)
        {
            errToSet = E_FS_FILE_IS_LOCKED;
        }
    }

    if (!errToSet)
    {
        int alreadyOpened = List_search(file->openedBy, List_getComparator(file->openedBy, NULL), &ownerId, &errToSet);
        if (!alreadyOpened)
        {
            errToSet = E_FS_FILE_NOT_OPENED;
        }
    }

    if (!errToSet)
    {
        // this functions invalidates the ability to write on the file
        file->ownerCanWrite.id = 0;

        resultFile = malloc(sizeof(*resultFile));
        IS_NULL_DO(resultFile, { errToSet = E_FS_MALLOC; })
    }

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

    if (!errToSet)
    {
        hasMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasMutex = 0;
                    })
    }

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

List_T FileSystem_readNFile(FileSystem fs, OwnerId ownerId, int N, int *error)
{
    int errToSet = 0;
    int hasFSMutex = 0;

    int counter = 0;
    int filesListLen = 0;
    List_T resultFiles = NULL;

    if (fs == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    if (!errToSet)
    {
        hasFSMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 0;
                    })
    }

    if (!errToSet)
    {
        filesListLen = List_length(fs->filesList, &errToSet);
    }

    if (!errToSet)
    {
        counter = (N >= filesListLen || N <= 0) ? filesListLen : N;
    }

    if (!errToSet)
    {
        resultFiles = List_create(NULL, NULL, fileResultDeallocator, &errToSet);
    }

    if (!errToSet)
    {
        struct ReadNFileSetResultFilesContext ctx;
        ctx.resultFiles = resultFiles;
        ctx.counter = counter;
        ctx.ownerId = ownerId;
        List_forEachWithContext(fs->filesList, readNFiles, &ctx, &errToSet);
    }

    if (!errToSet && fs->replacementPolicy == FS_REPLACEMENT_LRU)
    {
        struct ReadNFileLRUContext ctx;
        ctx.files = fs->filesList;
        List_forEachWithContext(resultFiles, moveFilesLRU, &ctx, &errToSet);
    }

    if (errToSet)
    {
        resultFiles ? List_free(&resultFiles, 0, NULL) : (void)NULL;
        resultFiles = NULL;
    }

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
        while ((file->activeReaders > 0 || file->activeWriters > 0) && !errToSet)
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

            // this functions invalidates the ability to write on the file
            file->ownerCanWrite.id = 0;
        }

        // if the LRU replacement policy was enabled, put this file on top of the list
        if (!errToSet && fs->replacementPolicy == FS_REPLACEMENT_LRU)
        {
            struct File temp;
            temp.path = file->path;
            List_insertHead(fs->filesList, List_searchExtract(fs->filesList, filePathComparator, &temp, NULL), &errToSet);
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
    if (!hasMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND)
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
        while ((file->activeReaders > 0 || file->activeWriters > 0) && !errToSet)
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

                if (!errToSet && !present)
                {
                    List_insertTail(file->waitingLockers, oid, &errToSet);
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

        // in case of LRU policy, put the file on top of the list
        if (!errToSet && fs->replacementPolicy == FS_REPLACEMENT_LRU)
        {
            struct File temp;
            temp.path = file->path;
            List_insertHead(fs->filesList, List_searchExtract(fs->filesList, filePathComparator, &temp, NULL), &errToSet);
        }
    }

    // acquire the file's mutex lock
    if (!hasMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND)
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
        while ((file->activeReaders > 0 || file->activeWriters > 0) && !errToSet)
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
            int lockedOwnerToSet = 0;

            // if someone else was waiting to set the lock on the file...
            if (!errToSet && listLength >= 1)
            {
                // ...extract it from the waiting list
                oidToRet = List_extractHead(file->waitingLockers, &errToSet);
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

        // in case of LRU policy, put the file on top of the list
        if (!errToSet && fs->replacementPolicy == FS_REPLACEMENT_LRU)
        {
            struct File temp;
            temp.path = file->path;
            List_insertHead(fs->filesList, List_searchExtract(fs->filesList, filePathComparator, &temp, NULL), &errToSet);
        }
    }

    // acquire the mutex lock of the file
    if (!hasMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND)
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
        while ((file->activeReaders > 0 || file->activeWriters > 0) && !errToSet)
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
            if (!errToSet)
            {
                //the "has opened the file?" control was passed, so 'ext' won't be NULL
                free(ext);
            }
        }

        // in case of LRU policy, put the file on top of the list
        if (!errToSet && fs->replacementPolicy == FS_REPLACEMENT_LRU)
        {
            struct File temp;
            temp.path = file->path;
            List_insertHead(fs->filesList, List_searchExtract(fs->filesList, filePathComparator, &temp, NULL), &errToSet);
        }
    }

    // acquire again the file's mutex
    if (!hasMutex && errToSet != E_FS_MUTEX && errToSet != E_FS_COND)
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

void FileSystem_removeFile(FileSystem fs, char *path, OwnerId ownerId, int *error)
{
    // is caller responsibility to free path
    // tengo la lock globale perche' devo distruggere le mutex del file e non voglio che altri thread
    // siano in attesa su esse

    int errToSet = 0;
    int hasFSMutex = 1;

    File file = NULL;

    if (fs == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    if (!errToSet && (path == NULL))
    {
        errToSet = E_FS_INVALID_ARGUMENTS;
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
        file = icl_hash_find(fs->filesDict, path);
        IS_NULL_DO(file, { errToSet = E_FS_FILE_NOT_FOUND; })
    }

    if (!errToSet)
    {
        NON_ZERO_DO(pthread_mutex_lock(&file->ordering),
                    {
                        errToSet = E_FS_MUTEX;
                    })
    }

    if (!errToSet)
    {
        NON_ZERO_DO(pthread_mutex_lock(&file->mutex),
                    {
                        errToSet = E_FS_MUTEX;
                    })
    }

    while ((file->activeReaders > 0 || file->activeWriters > 0) && !errToSet)
    {
        NON_ZERO_DO(pthread_cond_wait(&file->go, &file->mutex), {
            errToSet = E_FS_COND;
        })
    }

    if (!errToSet && file->currentlyLockedBy.id != ownerId.id)
    {
        errToSet = E_FS_FILE_IS_LOCKED;
    }

    if (!errToSet && file->currentlyLockedBy.id == 0)
    {
        errToSet = E_FS_FILE_NOT_LOCKED;
    }

    if (!errToSet && file->currentlyLockedBy.id == ownerId.id)
    {
        struct File temp;
        temp.path = path;
        file = List_searchExtract(fs->filesList, filePathComparator, &temp, &errToSet);
    }

    if (!errToSet)
    {
        // remove the file from the dict as well
        icl_hash_delete(fs->filesDict, file->path, NULL, NULL);
        // update fs
        fs->currentNumOfFiles--;
        fs->currentStorageSize -= file->size;

        free(file->data);
        free(file->path);
        List_free(&file->openedBy, 1, NULL);
        List_free(&file->waitingLockers, 1, NULL);
        free(file);
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
}

void FileSystem_evictClient(FileSystem fs, OwnerId ownerId, int *error)
{

    // tengo la lock globale per evitare modifiche indesiderate alle strutture
    // del file system che potrebbero portare a saltare file e/o peggiori errori
    // nell'iterazione

    int errToSet = 0;
    int hasFSMutex = 0;

    if (fs == NULL)
    {
        errToSet = E_FS_NULL_FS;
    }

    if (!errToSet)
    {
        hasFSMutex = 1;
        NON_ZERO_DO(pthread_mutex_lock(&fs->overallMutex),
                    {
                        errToSet = E_FS_MUTEX;
                        hasFSMutex = 0;
                    })
    }

    List_forEachWithContext(fs->filesList, evictClientInternal, &ownerId, &errToSet);

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

void ResultFile_free(ResultFile *rfPtr, int *error)
{
    int errToSet = 0;

    IS_NULL_DO(rfPtr, errToSet = E_FS_RESULTFILE_IS_NULL);

    if (!errToSet)
    {
        IS_NULL_DO(*rfPtr, errToSet = E_FS_RESULTFILE_IS_NULL);
    }

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
    int hasFSMutex = 1;
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
    int hasFSMutex = 1;
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
    }

    if (errToSet)
    {
        errToSet = E_FS_GENERAL;
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