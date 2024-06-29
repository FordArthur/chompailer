#ifndef CHOMPILER_INNER_TYPES
#define CHOMPILER_INNER_TYPES

#include <stdio.h>
#include <stdlib.h>

typedef enum ErrorType {
  SCANNER, PARSER, TYPECHECKER
} ErrorType;

typedef struct Error {
  ErrorType type;
  unsigned long index;
  unsigned long line;
  char* err;
} Error;

void report_error(Error err);


#define Malloc malloc
#define Free free
#define Realloc realloc

#endif  // !CHOMPILER_INNER_TYPES
