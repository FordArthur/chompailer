#ifndef CHORILANG_PARSER_HEADER
#define CHORILANG_PARSER_HEADER

#include "scanner.h"
#include "compiler_inner_types.h"

// !! Actual max precedence is halved !!
#define MAX_PRECEDENCE 256
#define MAX_PARENTHESIS 256

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
  F_DEFINITION,
  V_DEFINITION,
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
    struct ASTNode* expression; // first is function rest is arguments
    struct {
      struct ASTNode* operator;
      struct ASTNode* left_expression;
      struct ASTNode* right_expression;
    } bin_expression;
    struct {
      struct ASTNode* expression;
      struct ASTNode** type;
    } declaration;
    struct {
      char* name;
      struct {
        struct ASTNode* arguments;
        struct ASTNode* implementation;
      } body;
    } function_definition;
    struct {
      char* name;
      struct ASTNode* expression;
    } variable_definition;
  };
} ASTNode;

typedef struct AST {
  bool is_correct_ast;
  ASTNode* ast;
  Error* error_buf;
} AST;


void print_AST(ASTNode* ast);

AST parser(Token* tokens, Token** infixes, Error* error_buf);

#endif  // !CHORILANG_PARSER_HEADER
