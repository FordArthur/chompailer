#ifndef CHOMPILER_INNER_TYPES
#define CHOMPILER_INNER_TYPES

#include <stdio.h>
#include <stdlib.h>

#define mkerr(_type, _line, _index, _err) ((Error) {.type = _type, .line = _line, .index = _index, .err = _err}) 

typedef enum ErrorType {
  SCANNER, PARSER, TYPECHECKER
} ErrorType;

typedef struct Error {
  ErrorType type;
  unsigned long index;
  unsigned long line;
  char* err;
} Error;

void report_error(const Error* err, char** lines);

#define Malloc malloc
#define Free free
#define Realloc realloc

#endif  // !CHOMPILER_INNER_TYPES
