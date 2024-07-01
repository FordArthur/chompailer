#include "trie.h"

TrieNode* follow_pattern(char* pattern, TrieNode trie) {
  TrieNode* node = &trie;
  char* subpattern = pattern;
  for (TrieNode* child = node->first_child; child <= node->last_child; child++) {
    if (child->value != *subpattern)
      continue;

    node = child;
    child = child->first_child;
  }
  return node;
}
