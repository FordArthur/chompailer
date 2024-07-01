#ifndef CHORILANG_TRIE_HEADER
#define CHORILANG_TRIE_HEADER

#include <stdlib.h>

typedef struct TrieNode {
  char key;
  unsigned long value;
  struct TrieNode* first_child;
  struct TrieNode* last_child;
} TrieNode;

TrieNode* follow_pattern(char* pattern, TrieNode trie);

#endif  // !CHORILANG_TRIE_HEADER
