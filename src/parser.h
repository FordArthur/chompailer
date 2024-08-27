#ifndef CHORILANG_PARSER_HEADER
#define CHORILANG_PARSER_HEADER

#include <setjmp.h>
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
} TermType;

typedef enum ASTType {
  TERM,
  EXPRESSION,
  BIN_EXPRESSION,
  TYPE_ASSERTION,
  IMPLEMENTATION,
  V_DEFINITION,
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
      struct ASTNode* expression;
      struct ASTNode** constraints;
      struct ASTNode* type_v;
    } type_assertion;
    struct {
      struct ASTNode* lhs;
      struct ASTNode** body_v;
    } implementation;
    struct {
      struct ASTNode* type;
      struct ASTNode** constructors;
    } data_declaration;
    struct {
      struct ASTNode* name;
      struct ASTNode* expression;
    } variable_definition;
    struct {
      struct ASTNode* instance_class;
      struct ASTNode* instance_type;
      struct ASTNode** implementations_v;
    } instance_definition;
    struct {
      struct ASTNode* class_name;
      struct ASTNode** declarations_v;
    } class_declaration;
  };
} ASTNode;

typedef struct AST {
  bool is_correct_ast;
  ASTNode* ast;
  Error* error_buf;
} AST;

void print_AST(ASTNode* ast);


/** Parser rules:
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Note that this BNF-like syntax is less literal, "" refer to tokens with that name,        *
 * {} are literal, and () represent an output to the rule (that is info that we keep)        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Top-level:
 * - Declaration ::= (function name) :: <(types), `separated by comas and ended by an arrow`>
 * - Implementation ::= (function name) (<args>) = <(expression) `separated by comas and ended by a semicolon`>
 * - Class declaration ::= "class"  { <declaration> }
 */
AST parser(Token* tokens, Token** infixes, Error* error_buf);

#endif // !CHORILANG_PARSER_HEADER
