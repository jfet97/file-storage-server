#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#include "config-parser.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define MAX_CONFIG_LEN 256
#define MAX_KEY_LEN 124
#define MAX_VAL_LEN 124

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

typedef struct InternalListNode
{
    char key[MAX_KEY_LEN];
    char value[MAX_VAL_LEN];
    struct InternalListNode *next;
} InternalListNode;

struct ConfigParser
{
    InternalListNode *internal_list;
};

// -------------------------------
// -------------------------------
// INTERNALS

static int insert(InternalListNode **listPtr, InternalListNode *newNode)
{
    int codeToRet = 0;

    if (listPtr && newNode)
    {
        newNode->next = *listPtr;
        *listPtr = newNode;
    }
    else
    {
        codeToRet = -1;
    }

    return codeToRet;
}

static void forceFree(InternalListNode **listPtr)
{
    if (listPtr && *listPtr)
    {
        forceFree(&(*listPtr)->next);
        free((*listPtr));
    }
}

static void *searchByKey(InternalListNode *list, const char *key)
{
    if (list == NULL)
    {
        return NULL;
    }
    else if (strcmp(list->key, key) == 0)
    {
        return list->value;
    }
    else
    {
        return searchByKey(list->next, key);
    }
}

static int readLineFromFILE(char *buffer, unsigned int len, FILE *fp)
{
    int errToSet = 0;
    int feofRes = 0;

    char *res = fgets(buffer, len, fp);
    feofRes = feof(fp);

    if (res == NULL && feofRes == 0)
    {
        // error
        errToSet = -1;
    }

    if (res == NULL && feofRes != 0)
    {
        // reached eof
        errToSet = -2;
    }

    if (res)
    {
        /* remove the useless newline character at the end */
        char *newline = strchr(buffer, '\n');
        newline && (*newline = '\0');
    }

    return errToSet;
}

// -------------------------------
// -------------------------------
// API

ConfigParser ConfigParser_parse(const char *path, int *error)
{

    int errToSet = 0;
    ConfigParser parser = malloc(sizeof(*parser));
    FILE *file = NULL;

    IS_NULL_DO(parser, {
        errToSet = E_CP_MALLOC;
    })

    if (!errToSet)
    {
        parser->internal_list = NULL;

        file = fopen(path, "r");
        IS_NULL_DO(file, {
            errToSet = E_CP_FILE;
        })
    }

    if (!errToSet)
    {
        char buf[MAX_CONFIG_LEN];

        int read_res = readLineFromFILE(buf, MAX_CONFIG_LEN, file);

        while (read_res == 0 && !errToSet)
        {
            InternalListNode *newNode = malloc(sizeof(*newNode));

            IS_NULL_DO(newNode, {
                errToSet = E_CP_MALLOC;
            })

            if (newNode)
            {
                char *state = NULL;
                char *key = strtok_r(buf, ",", &state);
                char *value = strtok_r(NULL, ",", &state);
                char *end = strtok_r(NULL, ",", &state);

                newNode->key[0] = '\0';
                newNode->value[0] = '\0';

                if (!key || !value || end)
                {
                    errToSet = E_CP_MALFORMED_FILE;
                }
                else
                {
                    strncat(newNode->key, key, MAX_KEY_LEN);
                    strncat(newNode->value, value, MAX_VAL_LEN);
                }

                insert(&(parser->internal_list), newNode);
                read_res = readLineFromFILE(buf, MAX_CONFIG_LEN, file);
            }
        }

        if (read_res == -1)
        {
            errToSet = E_CP_FILE;
        }
    }

    if (file)
    {
        NON_ZERO_DO(fclose(file), {
            errToSet = E_CP_FILE;
        })
    }

    if (errToSet)
    {
        parser ? forceFree(&(parser->internal_list)) : (void)NULL;
        parser ? free(parser) : (void)NULL;
        parser = NULL;
    }

    error && (*error = errToSet);
    return parser;
}

void ConfigParser_delete(ConfigParser *parserPtr, int *error)
{
    int errToSet = 0;

    if (parserPtr == NULL)
    {
        errToSet = E_CP_NULL;
    }

    if (!errToSet)
    {
        forceFree(&((*parserPtr)->internal_list));
        free(*parserPtr);
        *parserPtr = NULL;
    }

    error && (*error = errToSet);
}

char *ConfigParser_getValue(ConfigParser parserPtr, const char *key, int *error)
{

    int errToSet = 0;
    char *toRet = NULL;

    if (parserPtr == NULL)
    {
        errToSet = E_CP_NULL;
    }

    if (!errToSet && key == NULL)
    {
        errToSet = E_CP_WRONG_INPUT;
    }

    if (!errToSet)
    {
        toRet = searchByKey(parserPtr->internal_list, key);
    }

    error && (*error = errToSet);
    return toRet;
}

void ConfigParser_printConfigs(ConfigParser parserPtr, int *error)
{
    int errToSet = 0;

    if (parserPtr == NULL)
    {
        errToSet = E_CP_NULL;
    }
    else
    {
        for (InternalListNode *runner = parserPtr->internal_list; runner; runner = runner->next)
        {
            printf("key: %s --- value: %s\n", runner->key, runner->value);
        }
    }

    error && (*error = errToSet);
}

const char *config_parser_error_messages[] = {
    "",
    "config parser internal malloc error",
    "config parser internal file error",
    "config parser is NULL",
    "config parser internal general error",
    "config parser has received wrong input",
    "the config file is malformed",
};

const char *ConfigParser_getErrorMessage(int errorCode)
{
    return config_parser_error_messages[errorCode];
}