#ifndef CHORILANG_TRIE_HEADER
#define CHORILANG_TRIE_HEADER

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define TRIE_LOOK_UP_SIZE 92
#define TRIE_ASCII_OFFSET 33
// This means we allow characters from 33 to 33 + 91 = 124

typedef struct TrieNode {
  char key;
  bool is_terminal;
  unsigned long value;
  struct TrieNode* children[TRIE_LOOK_UP_SIZE];
} TrieNode;

void _print_trie(TrieNode* trie, unsigned long spacing);
#define print_trie(t) _print_trie(t, 0)
TrieNode* create_node(char key, unsigned long value);
bool insert_trie(char* pattern, unsigned long final_value, TrieNode* trie);
bool insert_if_unique(char* pattern, unsigned long final_value, TrieNode* trie);
unsigned long follow_pattern_with_default(char* pattern, TrieNode* trie, unsigned long _default);
void for_each_in_trie(TrieNode* trie, void (*func)(unsigned long value));

#endif  // !CHORILANG_TRIE_HEADER
