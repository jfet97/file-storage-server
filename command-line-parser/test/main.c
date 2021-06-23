#include "command-line-parser.h"
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(int argc, char *argv[])
{

    Arguments as = CommandLineParser_parseArguments(argv + 1, NULL);

    CommandLineParser_printArguments(as, NULL);

    CommandLineParser_delete(&as, NULL);

    return 0;
}