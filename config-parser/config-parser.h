// parse a config.txt file
#ifndef CONFIG_PARSER__
#define CONFIG_PARSER__

#include <stddef.h>

#define E_CP_MALLOC 1
#define E_CP_FILE 2
#define E_CP_NULL 3
#define E_CP_ERROR 4
#define E_CP_WRONG_INPUT 5
#define E_CP_MALFORMED_FILE 6

typedef struct ConfigParser *ConfigParser;

ConfigParser ConfigParser_parse(const char *path, int *error);
void ConfigParser_delete(ConfigParser *parserPtr, int *error);
char *ConfigParser_getValue(ConfigParser parserPtr, const char *key, int *error);
void ConfigParser_printConfigs(ConfigParser parserPtr, int *error);
const char *ConfigParser_getErrorMessage(int errorCode);
#endif