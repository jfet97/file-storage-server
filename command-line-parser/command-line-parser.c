#include "command-line-parser.h"
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define WAIT_OP "WAIT_OP"
#define WAIT_PARAM "WAIT_PARAM"

struct Arguments
{
    Option *internal_list;
};

Option *Option_create(char op, char *param)
{
    Option *newOption = malloc(sizeof(*newOption));

    if (newOption)
    {
        newOption->op = op;
        newOption->param = param;
    }

    return newOption;
}

void Option_free(Option **optionPtr)
{
    free(*optionPtr);
    *optionPtr = NULL;
}

static void reverse(Option **nodePtr, Option* previous)
{

    if (*nodePtr == NULL)
    {
        // list with 0 elements
        return;
    }

    if ((*nodePtr)->next == NULL)
    {
        if (previous != NULL)
        {                                // list with more than one element
            (*nodePtr)->next = previous; // rotate the next pointer of the last element
        }
        return; // note: here *nodePtr is already the address of the last element aka the new head
    }

    // *nodePtr: my address
    // (*nodePtr)->next: address of the next element (before recursion)

    reverse(&(*nodePtr)->next, *nodePtr);

    // (*nodePtr)->next: address of the last element aka the new head (after recursion)
    Option *self = *nodePtr;
    *nodePtr = (*nodePtr)->next; // save the address of the new head inside the next field of my previous element or inside the head pointer
    self->next = previous;       // rotate my next pointer with the address of my previous element
}

static int insert(Option **listPtr, Option *newNode)
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

static void forceFree(Option **listPtr)
{
    if (listPtr && *listPtr)
    {
        forceFree(&(*listPtr)->next);
        Option_free(listPtr);
    }
}

static int isOpCountDashes(const char *str)
{
    int dashCounter = 0;

    if (str != NULL)
    {
        // leggo e conto i dash
        while (*str++ == '-')
        {
            dashCounter++;
        }
    }
    else
    {
        dashCounter = -1;
    }

    // if(dashCounter == 0) e' stato letto un param

    return dashCounter;
}

Arguments CommandLineParser_parseArguments(char *argv[], int *error)
{
    int errToSet = 0;
    const char *arg = NULL;
    const char *state = WAIT_OP;
    Arguments as = NULL;
    Option *option = NULL;

    if (argv == NULL)
    {
        errToSet = E_CLP_WRONG_INPUT;
    }

    if (!errToSet)
    {
        as = malloc(sizeof(*as));

        if (as == NULL)
        {
            errToSet = E_CLP_MALLOC;
        }

        as->internal_list = NULL;
    }

    // per ogni argomento
    while (!errToSet && (arg = *argv++) != NULL)
    {

        // determino lo stato corrente
        int is_WAIT_OP = strncmp(WAIT_OP, state, strlen(WAIT_OP)) == 0;
        int is_WAIT_PARAM = strncmp(WAIT_PARAM, state, strlen(WAIT_PARAM)) == 0;

        // conto il numero di dash per determinare cosa ho letto: op o param?
        int dashCounter = isOpCountDashes(arg);

        if (dashCounter == 0 && is_WAIT_OP)
        {
            // e' stato letto un param solitario da ignorare
            continue;
        }

        if (dashCounter == 0 && is_WAIT_PARAM)
        {
            // e' stato letto il param che attendevo
            option->param = arg;
            insert(&(as->internal_list), option);
            option = NULL;

            // successivamente aspetto una nuova op
            state = WAIT_OP;
            continue;
        }

        if (dashCounter != 0 && is_WAIT_PARAM)
        {
            // e' stata letta una nuova op quando attendevamo il param della precedente

            // param che allora non esiste
            option->param = NULL;
            insert(&(as->internal_list), option);
            option = NULL;
        }

        // ho letto una nuova op che trovo in arg+dashCounter
        arg += dashCounter;

        // puts(arg);

        // prelevo l'option
        const char op = *arg++;

        // printf("op %c\n", op);

        // creo la nuova option
        option = Option_create(op, NULL);

        if (!option)
        {
            errToSet = E_CLP_MALLOC;
            continue;
        }

        if (*arg == '\0')
        {
            // ho letto un opzione senza parametro che devo cercare di leggere nel prossimo argomento
            state = WAIT_PARAM;
            continue;
        }
        else
        {
            // prendo come parametro il rimanente dell'argomento
            option->param = arg;
            insert(&(as->internal_list), option);
            option = NULL;

            // attendo prossima op
            state = WAIT_OP;
            continue;
        }
    }

    if (!errToSet && option != NULL && strncmp(WAIT_PARAM, state, strlen(WAIT_PARAM)) == 0)
    {
        // è rimasta in sospeso una option senza parametro -> param sarà NULL
        option->param = NULL;
        insert(&(as->internal_list), option);
    }

    if (errToSet)
    {
        as ? forceFree(&(as->internal_list)) : (void)NULL;
        as ? free(as) : (void)NULL;
        option ? Option_free(&option) : (void)NULL;
        as = NULL;
    }
    else
    {
        // reverse the list
        reverse(&(as->internal_list), NULL);
    }

    error && (*error = errToSet);
    return as;
}

void CommandLineParser_delete(Arguments *argsPtr, int *error)
{
    int errToSet = 0;

    if (argsPtr == NULL || *argsPtr == NULL)
    {
        errToSet = E_CLP_NULL;
    }
    else
    {
        forceFree(&((*argsPtr)->internal_list));
        free((*argsPtr));
        *argsPtr = NULL;
    }

    error && (*error = errToSet);
}

void CommandLineParser_printArguments(Arguments args, int *error)
{
    int errToSet = 0;

    if (args == NULL)
    {
        errToSet = E_CLP_NULL;
    }
    else
    {
        for (Option *runner = args->internal_list; runner; runner = runner->next)
        {
            printf("option: %c --- param: %s\n", runner->op, runner->param);
        }
    }

    error && (*error = errToSet);
}

Option *CommandLineParser_getArguments(Arguments args, int *error)
{
    int errToSet = 0;
    Option *toRet = NULL;

    if (args == NULL)
    {
        errToSet = E_CLP_NULL;
    }
    else
    {
        toRet = args->internal_list;
    }

    error && (*error = errToSet);
    return toRet;
}

const char *logger_error_messages[] = {
    "",
    "command line parser internal malloc error",
    "command line parser is NULL",
    "command line parser internal general error",
    "command line parser has received wrong input",
};

const char *CommandLineParser_getErrorMessage(int errorCode)
{
    return logger_error_messages[errorCode];
}