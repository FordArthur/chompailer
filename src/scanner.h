#ifndef CHOMPAILER_SCANNER_HEADER
#define CHOMPAILER_SCANNER_HEADER

#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include "vec.h"
#include "trie.h"
#include "compiler_inner_types.h"

/**
 *  Things with !! TODO !! are to be implemented probably because they wouldn't
 *  be interpreted by later stages of the compiler (in this current version)
 **/

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
  // !! TODO !!
  LAMBDA, // Either \ or λ
  BAR,
  AMPERSAND,
  DOUBLE_BAR,
  DOUBLE_AMPERSAND,
  ARROW,
  DOUBLE_ARROW,
  // !! TODO !!
  CONSTANT_DEFINE, // :=
  IF,
  THEN,
  ELSE,
  LET,
  DATA,
  INSTANCE,
  CLASS,
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

/**
 *  Yes, most of the functions there could be derived using just one or two functions,
 *  but we'd rather the user provide for the most efficient version they have available
 **/

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
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Note that this BNF-like syntax is space sensitive, <> mean repetition, [] mean set of     *
 * posible occurences, and names refer to previous rules or trivial sets (like `chars`)      *
 * (It's not formal at all, i know)                                                          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Numbers:
 * - Naturals ::= <[0-9]>
 * - Reals ::= Naturals.Naturals
 * Strings:
 * - String literals ::= "<[chars - "]>"
 * Identifiers:
 * - Identifiers ::= (alphanumerics - [A-Z]) <alphanumerics>
 * - Types ::= [A-Z] <alphanumerics>
 * - Operator ::= <ºª!·$%&/=?¿^+*<>,.:-_|@#~½¬•>
 */

Tokens scanner(Stream stream);

void print_token(Token* tok);

/*** Possible improvements:
 * - If possible, we shouldn't need to allocate space for each token we see and instead just provide where
 *   it occurs in the stream and the token's size (perhaps replacing strntok for strtok and doing replacing chars with \0 when printing)
 * - Use an arena allocator
 ***/

#endif  // !CHOMPAILER_SCANNER_HEADER
