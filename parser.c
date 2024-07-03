#include "parser.h"
#include "scanner.h"
#include "vec.h"

inline unsigned long get_arg_length(char* name) {
  // ...
  return 0;
}

AST parser(Token* tokens, Priority* infixes) {
  Token* curexp = new_vector_with_capacity(*curexp, 8);
  Token* leftexp = new_vector_with_capacity(*curexp, 8);
  Token* rightexp = new_vector_with_capacity(*curexp, 8);
  // Build table for infixes

  // Iterate over tokens
  while (tokens->type != _EOF) {
    switch (tokens->type) {
      case OPEN_PAREN:

      case CLOSE_PAREN:
      case CLOSE_BRACKET:
      case OPEN_BRACKET:
      case OPEN_CURLY:
      case CLOSE_CURLY:
        break;
      case IDENTIFIER:
        for (unsigned long i = 0; i < get_arg_length(tokens->token); i++) {

        }
        break;
    }
  }
}
