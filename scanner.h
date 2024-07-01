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

  SEMI_COLON,
  DOUBLE_COLON,
  COMA,
  ARROW,
  DOUBLE_ARROW,
  EQUALS,
  LAMBDA, // Either \ or λ

  NATURAL, REAL,
  CHARACTER, STRING,
  IDENTIFIER, OPERATOR, TYPE,

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

typedef struct InfixIndicator {
  unsigned short precedence;
  bool is_infixr;
} InfixIndicator;

typedef struct Tokens {
  bool is_correct_stream;
  union {
    struct {
      Token* token_stream;
      InfixIndicator* infixes;
      char** lines;
    } scanned;
    Error* errors;
  };
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
