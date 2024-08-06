#ifndef CHOMPAILER_SCANNER_HEADER
#define CHOMPAILER_SCANNER_HEADER

#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include "vec.h"
#include "trie.h"
#include "compiler_inner_types.h"

typedef enum TokenType {
  OPEN_PAREN, CLOSE_PAREN,
  OPEN_BRACKET, CLOSE_BRACKET,
  OPEN_CURLY, CLOSE_CURLY,

  NATURAL, REAL,
  CHARACTER, STRING,
  IDENTIFIER, OPERATOR, TYPE_K,

  SEMI_COLON,
  DOUBLE_COLON,
  COMA,
  EQUALS,
  LAMBDA, // Either \ or λ
  BAR,
  AMPERSAND,
  DOUBLE_BAR,
  DOUBLE_AMPERSAND,
  ARROW,
  DOUBLE_ARROW,
  CONSTANT_DEFINE, // :=
  IF,
  THEN,
  ELSE,
  LET,
  DATA,
  INSTANCE,
  CLASS, // Not supported, for now
  INFIXL,
  INFIXR,

  COMMENT,

  EndOfFile // Necessary because parser needs a final delimiting token (enables multithreading too in a future)
} TokenType;

typedef struct Token {
  TokenType type;
  unsigned long index;
  unsigned long line;
  unsigned long length;
  char* token;
} Token;

typedef struct Tokens {
  bool is_correct_stream;
  void** lines;
  struct {
    Token* token_stream;
    Token** infixes;
  } scanned;
  Error* error_buf;
} Tokens;

typedef struct Stream {
  void* stream;
  char (*consume_char)(void**);
  char (*get_char)(void*);
  char (*look_around)(void*, long);
  void (*move_stream)(void**, long);
  void* (*copy_stream_offset)(void*, long);
  unsigned long (*distance_between)(void*, void*);
} Stream;

/** Scanner rules:
 * Numbers:
 * - Naturals ::= <[0-9]>
 * - Reals ::= Naturals.Naturals
 * Strings:
 * - String literals ::= "<[Chars - "]>"
 * Identifiers:
 * - Identifiers ::= (alphanumerics - [A-Z]) <alphanumerics>
 * - Types ::= [A-Z] <alphanumerics>
 * - Operator ::= <ºª!·$%&/=?¿^+*<>,.:-_|@#~½¬•>
 */

Tokens scanner(Stream stream);

void print_token(Token* tok);

#endif  // !CHOMPAILER_SCANNER_HEADER
