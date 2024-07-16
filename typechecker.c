#include "typechecker.h"
#include "trie.h"

static bool passes_type_check = true;
static TrieNode* type_trie;

static void check(unsigned long _def) {
  ASTNode* def = (ASTNode*) _def;
}

bool checker(TrieNode* astrie, Error* error_buf) {
  // Initialize class table & predefined types
  // ...
  
  for_each_in_trie(astrie, check);
  return passes_type_check;
}
