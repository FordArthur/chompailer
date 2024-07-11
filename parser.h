#ifndef CHORILANG_PARSER_HEADER
#define CHORILANG_PARSER_HEADER

#include "trie.h"
#include "scanner.h"
#include "compiler_inner_types.h"

// !! Actual max precedence is halved !!
#define MAX_PARENTHESIS 255

typedef enum TermType {
  FUNCTION,

  TNATURAL,
  TREAL,
  TCHARACTER,
  TSTRING,
  TYPE_CONSTRUCTOR,

  TYPE,
} TermType;

typedef enum ASTType {
  TERM,
  EXPRESSION,
  BIN_EXPRESSION,
  DECLARATION,
  IMPLEMENTATION,
  V_DEFINITION,
  F_DEFINITION,
} ASTType;

typedef struct ASTNode {
  ASTType type;
  union {
    struct {
      TermType type;
      unsigned long index;
      unsigned long line;
      unsigned long length;
      char* name;
    } term;
    struct ASTNode* expression; // First is function rest is arguments
    struct {
      struct ASTNode* op;
      struct ASTNode* left_expression;
      struct ASTNode* right_expression;
    } bin_expression;
    struct {
      struct ASTNode* expression;
      struct ASTNode* type;
    } declaration;
    struct {
      struct ASTNode* arguments;
      struct ASTNode* body;
    } implementation;
    struct {
      struct ASTNode* declaration;
      struct ASTNode* implementations;
    } function_definition;
    struct {
      struct ASTNode* arguments; // Null if none, i.e. if value
      struct ASTNode* expression;
    } variable_definition;
  };
} ASTNode;

typedef struct AST {
  bool is_correct_ast;
  TrieNode* astrie;
  Error* error_buf;
} AST;

#define print_AST(ast) _print_AST(ast, 0)
void _print_AST(volatile ASTNode* ast, unsigned long spacing);

AST parser(Token* tokens, Token** infixes, Error* error_buf);

#endif // !CHORILANG_PARSER_HEADER
