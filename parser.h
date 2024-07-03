#ifndef CHORILANG_PARSER_HEADER
#define CHORILANG_PARSER_HEADER

#include "scanner.h"
#include "compiler_inner_types.h"

typedef enum TermType {
  FUNCTION,
  LITERAL,
  VARIABLE,

  TYPE,

  TYPE_CONSTRUCTOR,
} TermType;

typedef enum ASTType {
  TERM,
  EXPRESSION,
  DECLARATION,
  F_DEFINITION,
  V_DEFINIITON,
} ASTType;

typedef struct ASTNode {
  ASTType type;
  union {
    struct {
      TermType type;
      char* name;
    } term;
    struct ASTNode* expression; // first is function rest is arguments
    struct {
      struct ASTNode* expression;
      struct ASTNode* type;
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
  union {
    ASTNode* ast;
    Error* errors;
  };
} AST;

AST parser(Token* tokens, Priority* infixes);

#endif  // !CHORILANG_PARSER_HEADER
