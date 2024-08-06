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

  TYPE,
} TermType;

typedef struct Type Type;

typedef enum ASTType {
  TERM,
  EXPRESSION,
  BIN_EXPRESSION,
  FORMAL_TYPE,
  DECLARATION,
  IMPLEMENTATION,
  V_DEFINITION,
  F_DEFINITION,
  CONSTRUCTOR_DECLARATION,
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
      Type** type_v_v;
    } formal_type;
    struct {
      struct ASTNode* expression_v;
      struct ASTNode* formal_type;
    } declaration;
    struct {
      struct ASTNode* arguments_v;
      struct ASTNode** body_v;
    } implementation;
    struct {
      Type* declaration;
      struct ASTNode* implementations_v;
    } function_definition;
    struct {
      struct ASTNode** constraints_v_v;
      Type** constructor_type_v_v;
    } constructor_declaration;
    struct ASTNode* type_arguments_v;
    struct {
      struct ASTNode* arguments_v;
      struct ASTNode* expression;
    } variable_definition;
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

typedef enum TypeKind {
  CONSTRUCTOR, VAR, CONCRETE, FUNC
} TypeKind;

typedef struct Type {
  TypeKind kind;
  union {
    struct Type* constructor;
    ASTNode* concrete;
    struct {
      // Note: since pointers are either 64 bits, in which case it needs to be 8 byte aligned (so 2 ints),
      // or 32 bits, in which case it needs to be 4 byte aligned (2 ints again) we can just give identifier int,
      // using space that would otherwise be lost
      int identifier;
      ASTNode* constraints;
    } var;
    struct Type* func;
  };
} Type;

typedef struct AST {
  bool is_correct_ast;
  TrieNode* astrie;
  TrieNode* type_trie;
  TrieNode* instance_trie;
  Error* error_buf;
} AST;

void print_AST(ASTNode* ast);

AST parser(Token* tokens, Token** infixes, Error* error_buf);

#endif // !CHORILANG_PARSER_HEADER
