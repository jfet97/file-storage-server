#define _POSIX_C_SOURCE 200809L
#include "logger.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

#define MAX_LOG_SIZE 131072
#define MEDIUM_LOG_SIZE 16384
#define INITIAL_LOG_SIZE 1024
#define TIMESTAMP_LEN 26

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

#define IS_NEGATIVE_DO(code, todo) \
    if (code < 0)                  \
    {                              \
        todo;                      \
    }

#define IS_NEGATIVE_DO_ELSE(code, todo, el_se) \
    if (code < 0)                              \
    {                                          \
        todo;                                  \
    }                                          \
    else                                       \
    {                                          \
        el_se;                                 \
    }

struct Logger
{
    char *log;
    FILE *file;
    size_t curr_log_len;
    size_t curr_log_size;
};

// it's a singleton
Logger logger = NULL;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static int realloca(char **buf, size_t newsize)
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

void Logger_create(const char *path, int *error)
{
    int errToSet = 0;
    int hasLoggerLock = 1;

    NON_ZERO_DO(pthread_mutex_lock(&lock),
                {
                    errToSet = E_LOG_MUTEX;
                    hasLoggerLock = 0;
                })

    if (!errToSet && logger != NULL)
    {
        errToSet = E_LOG_SINGLETON;
    }

    if (!errToSet)
    {
        IS_NULL_DO(path, {
            errToSet = E_LOG_WRONG_INPUT;
        })
    }

    if (!errToSet)
    {
        logger = malloc(sizeof(*logger));

        IS_NULL_DO(logger, {
            errToSet = E_LOG_MALLOC;
        })
    }

    if (!errToSet)
    {
        logger->log = malloc(sizeof(*(logger->log)) * INITIAL_LOG_SIZE);

        IS_NULL_DO(logger->log, {
            errToSet = E_LOG_MALLOC;
        })

        logger->log[0] = '\0';
        logger->curr_log_size = INITIAL_LOG_SIZE;
        logger->curr_log_len = 0;
    }

    if (!errToSet)
    {
        logger->file = fopen(path, "w");
        IS_NULL_DO(logger->file, {
            errToSet = E_LOG_FILE;
        })
    }

    if (hasLoggerLock && errToSet && errToSet != E_LOG_SINGLETON && errToSet != E_LOG_WRONG_INPUT)
    {
        (logger && logger->log) ? free(logger->log) : (void)NULL;
        logger ? free(logger) : (void)NULL;
        logger = NULL;
    }

    if (hasLoggerLock)
    {
        hasLoggerLock = 0;
        NON_ZERO_DO(pthread_mutex_unlock(&lock),
                    {
                        errToSet = E_LOG_MUTEX;
                        hasLoggerLock = 1;
                    })
    }

    error && (*error = errToSet);
}

void Logger_delete(int force_free, int *error)
{
    int errToSet = 0;
    int hasLoggerLock = 0;

    IS_NULL_DO(logger, {
        errToSet = E_LOG_NULL;
    })

    if (!errToSet)
    {
        hasLoggerLock = 1;
        NON_ZERO_DO(pthread_mutex_lock(&lock),
                    {
                        errToSet = E_LOG_MUTEX;
                        hasLoggerLock = 0;
                    })
    }

    if (hasLoggerLock)
    {
        IS_NEGATIVE_DO_ELSE(
            fprintf(logger->file, "%s", logger->log),
            errToSet = E_LOG_FILE;
            ,
            logger->log[0] = '\0';)

        // chiudo anche se e' fallita la flush
        int closeFileSuccessfull = 1;
        NON_ZERO_DO(fclose(logger->file),
                    {
                        errToSet = E_LOG_FILE;
                        closeFileSuccessfull = 0;
                    })

        // non chiudo se non sono riuscito a chiudere il file
        // a meno che non sono forzato
        if (closeFileSuccessfull == 1 || force_free)
        {
            free(logger->log);
            free(logger);
            logger = NULL;
        }

        NON_ZERO_DO(pthread_mutex_unlock(&lock), {
            errToSet = E_LOG_MUTEX;
        })
    }

    error && (*error = errToSet);
}

void Logger_log(const char *toLog, size_t len, int *error)
{
    int errToSet = 0;
    int hasLoggerLock = 0;
    time_t ltime;
    struct tm *local_time = malloc(sizeof(*local_time));
    char timestamp[26];

    IS_NULL_DO(logger, {
        errToSet = E_LOG_NULL;
    })

    IS_NULL_DO(local_time, {
        errToSet = E_LOG_MALLOC;
    })

    if (!errToSet)
    {
        IS_NULL_DO(toLog, {
            errToSet = E_LOG_WRONG_INPUT;
        })
    }

    if (!errToSet)
    {
        hasLoggerLock = 1;
        NON_ZERO_DO(pthread_mutex_lock(&lock),
                    {
                        errToSet = E_LOG_MUTEX;
                        hasLoggerLock = 0;
                    })
    }

    if (!errToSet)
    {

        errno = 0;
        ltime = time(NULL);
        NON_ZERO_DO(errno, {
            errToSet = E_LOG_TIME;
        })
    }

    if (!errToSet)
    {
        errno = 0;
        localtime_r(&ltime, local_time);
        NON_ZERO_DO(errno, {
            errToSet = E_LOG_TIME;
        })
    }

    if (!errToSet)
    {
        asctime_r(local_time, timestamp);

        IS_NULL_DO(timestamp, {
            errToSet = E_LOG_TIME;
        })
    }

    if (!errToSet)
    {
        size_t timestamp_len = TIMESTAMP_LEN;
        size_t len_to_write = logger->curr_log_len + (len + timestamp_len + 2);
        size_t new_realloc_size = len_to_write * 1.6180339887;
        int do_cat = 0;

        if (len_to_write < logger->curr_log_size)
        {
            do_cat = 1;
        }
        else if (len_to_write >= MAX_LOG_SIZE || new_realloc_size >= MAX_LOG_SIZE)
        {
            IS_NEGATIVE_DO(
                fprintf(logger->file, "%s%s %s", logger->log, toLog, timestamp), {
                    errToSet = E_LOG_FILE;
                })

            if (!errToSet)
            {
                free(logger->log);
                logger->curr_log_len = 0;
                logger->curr_log_size = 0;

                logger->log = malloc(sizeof(*logger->log) * MEDIUM_LOG_SIZE);

                IS_NULL_DO(logger, {
                    errToSet = E_LOG_NULL;
                })
            }

            if (!errToSet)
            {
                logger->log[0] = '\0';
                logger->curr_log_size = MEDIUM_LOG_SIZE;
            }
        }
        else
        {
            NON_ZERO_DO(realloca(&(logger->log), new_realloc_size), {
                errToSet = E_LOG_MALLOC;
            })

            if (!errToSet)
            {
                do_cat = 1;
                logger->curr_log_size = new_realloc_size;
            }
        }

        if (do_cat)
        {
            strncat(logger->log, toLog, len);
            strcat(logger->log, " ");
            strncat(logger->log, timestamp, timestamp_len);
            logger->curr_log_len += len_to_write;
        }
    }

    if (hasLoggerLock)
    {
        NON_ZERO_DO(pthread_mutex_unlock(&lock), {
            errToSet = E_LOG_MUTEX;
        })
    }

    error && (*error = errToSet);
}

void Logger_flush(int *error)
{
    int errToSet = 0;
    int hasLoggerLock = 0;

    IS_NULL_DO(logger, {
        errToSet = E_LOG_NULL;
    })

    if (!errToSet)
    {
        hasLoggerLock = 1;
        NON_ZERO_DO(pthread_mutex_lock(&lock),
                    {
                        errToSet = E_LOG_MUTEX;
                        hasLoggerLock = 0;
                    })
    }

    if (hasLoggerLock)
    {

        IS_NEGATIVE_DO_ELSE(
            fprintf(logger->file, "%s", logger->log),
            errToSet = E_LOG_FILE;
            ,
            logger->log[0] = '\0';)

        NON_ZERO_DO(pthread_mutex_unlock(&lock), {
            errToSet = E_LOG_MUTEX;
        })
    }

    error && (*error = errToSet);
}

const char *logger_error_messages[] = {
    "",
    "log internal malloc error",
    "log internal file error",
    "log internal general error",
    "log internal mutex lock error",
    "log has received wrong input",
    "log instance already exists",
    "log is NULL",
    "log internal time error",
};

inline const char *Logger_getErrorMessage(int errorCode)
{
    return logger_error_messages[errorCode];
}