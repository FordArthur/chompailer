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
  HIGHER_ORDER_TYPE,

  TYPE,
} TermType;

typedef enum ASTType {
  TERM,
  EXPRESSION,
  BIN_EXPRESSION,
  FORMAL_TYPE,
  DECLARATION,
  IMPLEMENTATION,
  V_DEFINITION,
  F_DEFINITION,
  DATA_DECLARATION,
  INSTANCE_DEFINITION,
  CLASS_DECLARATION,
} ASTType;

// Fields suffixed with _v are vectors
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
    struct ASTNode* expression_v;
    struct {
      struct ASTNode* op;
      struct ASTNode* left_expression_v;
      struct ASTNode* right_expression_v;
    } bin_expression;
    struct {
      struct ASTNode** constraints_v_v;
      struct ASTNode* type_v;
    } formal_type;
    struct {
      struct ASTNode* expression_v;
      struct ASTNode* type_v;
    } declaration;
    struct {
      struct ASTNode* arguments_v;
      struct ASTNode* body_v;
    } implementation;
    struct {
      struct ASTNode* declaration;
      struct ASTNode* implementations_v;
    } function_definition;
    struct {
      struct ASTNode* arguments_v;
      struct ASTNode* expression;
    } variable_definition;
    struct {
      struct ASTNode* constraints;
      struct ASTNode** decl_v;
    } data_declaration;
    struct {
      struct ASTNode* instance_type;
      struct ASTNode* implementations_v;
    } instance_definition;
    struct {
      struct ASTNode* instance_type;
      struct ASTNode* declarations_v;
    } class_declaration;
  };
} ASTNode;

typedef struct AST {
  bool is_correct_ast;
  TrieNode* astrie;
  TrieNode* type_trie;
  Error* error_buf;
} AST;

void print_AST(ASTNode* ast);

AST parser(Token* tokens, Token** infixes, Error* error_buf);

#endif // !CHORILANG_PARSER_HEADER
