#include "trie.h"
#include <stdbool.h>

void _print_trie(TrieNode* trie, unsigned long spacing) {
  if (!trie)
    return;
  for (unsigned long pad = spacing; pad > 0; pad--)
    printf("\t");
  printf("(%c : %lu)\n", trie->key, trie->value);
  for (unsigned long i = 0; i < TRIE_LOOK_UP_SIZE; i++)
    _print_trie(trie->children[i], spacing + 1);
}

void for_each_in_trie(TrieNode* trie, void (*func)(unsigned long value)) {
  if (!trie)
    return;
  if (trie->value != (unsigned long) -1)
    (*func)(trie->value);
  for (unsigned long i = 0; i < TRIE_LOOK_UP_SIZE; i++)
    for_each_in_trie(trie->children[i], func);
}

TrieNode* create_node(char key, unsigned long value) {
  TrieNode* new_node = malloc(sizeof(*new_node));
  new_node->key = key;
  new_node->value = value;
  for (unsigned long i = 0; i < TRIE_LOOK_UP_SIZE; i++)
    new_node->children[i] = 0;
  return new_node;
}

// Returns false if it didn't insert the pattern (already there / not enough memory)
bool insert_trie(char* pattern, unsigned long final_value, TrieNode* trie) {
  TrieNode* node = trie;
  char* subpattern = pattern;
  bool is_new = false;
  for (; *subpattern; subpattern++) {
    if (TRIE_ASCII_OFFSET > *subpattern || *subpattern > TRIE_ASCII_OFFSET + TRIE_LOOK_UP_SIZE)
      return false;
    if (!node->children[*subpattern - TRIE_ASCII_OFFSET]) {
      is_new = true;
      node->children[*subpattern - TRIE_ASCII_OFFSET] = create_node(*subpattern, subpattern[1]? -1 : final_value);
    }
    node = node->children[*subpattern - TRIE_ASCII_OFFSET];
  }
  return true;
}

// Returns the key or -1 if it didn't arrive to a terminal node
unsigned long follow_pattern_with_default(char* pattern, TrieNode* trie, unsigned long _default) {
  TrieNode* node = trie;
  char* subpattern = pattern;
  for (; *subpattern; subpattern++) {
    if (!node->children[*subpattern - TRIE_ASCII_OFFSET])
      return _default;
    node = node->children[*subpattern - TRIE_ASCII_OFFSET];
  }
  return node->value == -1? _default : node->value;
}
