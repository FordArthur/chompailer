#ifndef CHORILANG_TYPECHECKER_HEADER
#define CHORILANG_TYPECHECKER_HEADER


#include "compiler_inner_types.h"
#include "parser.h"
#include "trie.h"


// Returns whether or not the ASTrie has correct typing and scoping, and name-mangles type class methods of the ASTRie
bool checker(ASTNode* ast, Error* error_buf);

#endif  // CHORILANG_TYPECHECKER_HEADER
