#include "../inc/dotenv.h"
#include "../inc/C/errors.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

dotenv_array_t *read_dot_env(EMPTY) {
  char *equals_sign, *name, *value;
  char line[LINE_SIZE];
  size_t count = INIT_COUNT;
  size_t cap = INIT_CAP; 
  dotenv_t *items = malloc(cap * sizeof(dotenv_t));
  dotenv_array_t *result = malloc(sizeof(dotenv_array_t));
  FILE *file = fopen(DOTENV_FILE, READ_PERMISSIONS);
  
  if (!items) 
    ERROR_EXIT("Memory allocation failed for items");
  if (!file) 
    ERROR_EXIT("Error reading dotenv file, make sure it exists");
  if (!result) 
    ERROR_EXIT("Memory allocation failed for result struct");

  while (fgets(line, sizeof(line), file)) {
      line[strcspn(line, "\r\n")] = 0; 

      if (line[0] == '\0' || line[0] == '#') 
        continue; 

      equals_sign = strchr(line, '=');
      
      if (!equals_sign) 
        continue;

      *equals_sign = '\0';
      name = line;
      value = equals_sign + 1;

      if (count >= cap) {
          cap *= INCREASE_CAP;
          items = realloc(items, cap * sizeof(dotenv_t));
          if (!items) 
            ERROR_EXIT("Memory reallocation failed for items");
      }

      items[count].name = strdup(name);
      items[count].value = strdup(value);
      count++;
  }

  fclose(file);


  result->items = items;
  result->count = count;
  return result;
}

void destroy_dotenv_array(IN dotenv_array_t *array) {
    if (!array) return;
    for (size_t i = 0; i < array->count; i++) {
        free(array->items[i].name);
        free(array->items[i].value);
    }
    free(array->items);
    free(array);
}
