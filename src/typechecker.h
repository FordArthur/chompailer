#ifndef CHORILANG_TYPECHECKER_HEADER
#define CHORILANG_TYPECHECKER_HEADER


#include "compiler_inner_types.h"
#include "parser.h"
#include "trie.h"

// Returns whether or not the ASTrie has correct typing and scoping, and name-mangles type class methods of the ASTRie
bool checker(TrieNode* astrie, TrieNode* _type_trie, TrieNode* _instance_trie, Error* error_buf);

#endif  // CHORILANG_TYPECHECKER_HEADER