#ifndef COMMAND_LINE_PARSER__
#define COMMAND_LINE_PARSER__

#include <stddef.h>

#define E_CLP_MALLOC 1
#define E_CLP_NULL 2
#define E_CLP_ERROR 3
#define E_CLP_WRONG_INPUT 4

typedef struct Arguments *Arguments;

typedef struct Option
{
    char op;
    const char *param;
    struct Option *next;
} Option;

const char *CommandLineParser_getErrorMessage(int errorCode);
Arguments CommandLineParser_parseArguments(char *argv[], int *error);
Option *CommandLineParser_getArguments(Arguments args, int *error);
void CommandLineParser_printArguments(Arguments args, int *error);
void CommandLineParser_delete(Arguments *argsPtr, int *error);

#endif