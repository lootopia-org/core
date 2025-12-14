#ifndef DOTENV_H
#define DOTENV_H

#include "../inc/C/arguments.h"
#include <stddef.h>

#define DOTENV_FILE ".env"
#define READ_PERMISSIONS "r"

#define LINE_SIZE 1024
#define INIT_CAP 10
#define INIT_COUNT 0
#define INCREASE_CAP 2

typedef struct DOTENV {
    char *name;
    char *value;
} dotenv_t;

typedef struct DOTENV_ARRAY {
    dotenv_t *items;
    size_t count;
} dotenv_array_t;


dotenv_array_t *read_dot_env(EMPTY);

void destroy_dotenv_array(IN dotenv_array_t *array);

#endif
