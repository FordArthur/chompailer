#ifndef CHOMPAILER_SCANNER
#define CHOMPAILER_SCANNER

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

  LINE,
  SEMI_COLON,
  DOUBLE_COLON,
  COMA,
  EQUALS,
  LAMBDA, // Either \ or λ
  BAR,
  STAR,
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

  COMMENT,

  _EOF
} TokenType;

typedef struct Token {
  TokenType type;
  unsigned long index;
  unsigned long line;
  unsigned long length;
  char* token;
} Token;

typedef Token* Priority;

typedef struct Tokens {
  bool is_correct_stream;
  char** lines;
  struct {
    Token* token_stream;
    Priority* infixes;
  } scanned;
  Error* error_buf;
} Tokens;

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

Tokens scanner(char* stream);

#endif  // !CHOMPAILER_SCANNER
