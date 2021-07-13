// thread safe logger to log text strings into a custom output file
//
// the logger uses a buffer of dynamic size, on RAM, to save the strings to be logged,
// buffer that is periodically flushed but can be manually flushed as well
#ifndef LOGGER__
#define LOGGER__

#include <stddef.h>

#define E_LOG_MALLOC 1
#define E_LOG_FILE 2
#define E_LOG_ERROR 3
#define E_LOG_MUTEX 4
#define E_LOG_WRONG_INPUT 5
#define E_LOG_SINGLETON 6
#define E_LOG_NULL 7
#define E_LOG_TIME 8


typedef struct Logger *Logger;

void Logger_create(const char *path, int *error);
void Logger_delete(int force_free, int *error);
void Logger_log(const char *toLog, size_t len, int *error);
void Logger_flush(int *error);
const char *Logger_getErrorMessage(int errorCode);
#endif