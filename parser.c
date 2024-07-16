#include "parser.h"
#include "compiler_inner_types.h"
#include "scanner.h"
#include "trie.h"
#include "vec.h"
#include <stdio.h>

#define upper_ptr_bits 0xFFFF000000000000
#define elemsof(xs) (sizeof(xs)/sizeof(xs[0]))
#define bool_lambda(name, expr) static inline bool name(TokenType t) { return (expr); }
#define token_to_term(termtype, tok) (ASTNode) {.type = TERM, .term.type = termtype, .term.index = tok.index, .term.line = tok.line, .term.length = tok.length, .term.name = tok.token}

// !! It's really important that the size of this struct isnt greater than unsigned long !!
typedef struct PrecInfo {
  unsigned int prec;
  bool is_infixr;
} PrecInfo;

typedef unsigned char ExpStackIndex;

static inline bool in_delims(TokenType type, TokenType* dels, unsigned long dels_s);

static TokenType
comas[] = {
  COMA
}, close_constaint[] = {
  CLOSE_PAREN,
  DOUBLE_ARROW
}, equals[] = {
  EQUALS
}, bars[] = {
  BAR
}, semicolon[] = {
  SEMI_COLON
}, arrow_to_type[] = {
  ARROW,
  TYPE_K
}, value_expression[] = {
  TYPE_K, IDENTIFIER, NATURAL, REAL, CHARACTER, STRING, 
}, identifier[] = {
  IDENTIFIER
};

bool_lambda(identifiers, (t == IDENTIFIER));
bool_lambda(not_identifiers, (t != IDENTIFIER));
bool_lambda(iden_and_types, (t == TYPE_K || t == IDENTIFIER));
bool_lambda(not_iden_and_types, (t != TYPE_K && t != IDENTIFIER));
bool_lambda(value, (in_delims(t, value_expression, elemsof(value_expression))))
bool_lambda(non_value, (!in_delims(t, value_expression, elemsof(value_expression))))


#ifdef DEBUG
#pragma GCC push_options
#pragma GCC optimize ("O0")
#endif

static inline void* encode_error(void* error_ptr) {
  return (void*) ((unsigned long) error_ptr | upper_ptr_bits);
}

static inline bool test_error(void* ptr) {
  return ((unsigned long) ptr & upper_ptr_bits) >> 63;
}

// True <==> Error
static inline bool decode_error(void** ptr_ptr) {
  void* prev_ptr = *ptr_ptr;
  *ptr_ptr = (void*) ((unsigned long) prev_ptr & ~upper_ptr_bits);
  return prev_ptr != *ptr_ptr;
}

void print_AST(ASTNode* ast) {
  if (!ast) {
    printf(":: To be inferred");
    return;
  }
  switch (ast->type) {
    case TERM:
      printf("%s", ast->term.name);
      break;
    case EXPRESSION:
      printf("(");
      for_each(i, ast->expression_v) {
        print_AST(ast->expression_v + i);
        printf(" ");
      }
      printf("\b)");
      break;
    case BIN_EXPRESSION:
      printf("%s (", ast->bin_expression.op->term.name);
      print_AST(ast->bin_expression.left_expression_v);
      printf(") (");
      print_AST(ast->bin_expression.right_expression_v);
      printf(")");
      break;
    case DECLARATION:
      if (ast->declaration.expression_v) for_each(i, ast->declaration.expression_v) {
        print_AST(ast->declaration.expression_v + i);
        printf(" ");
      }
      printf(":: ");
      // This is a for_each, i have 0 clue why it thinks there are 102 elements if i dont 
      // do it this way
      for (unsigned long i = 0; i < _get_header(ast->declaration.type_v_v)->size; i++) { 
        for (unsigned long j = 0; j < _get_header(ast->declaration.type_v_v)->size; j++) {
          print_AST(ast->declaration.type_v_v[i] + j);
          printf(" ");
        }
        printf("\b, ");
      }
      printf("\b\b  ");
      break;
    case V_DEFINITION:
    case IMPLEMENTATION:
      for_each(i, ast->implementation.arguments_v) {
        print_AST(ast->implementation.arguments_v + i);
        printf(" ");
      }
      printf("= ");
      if (ast->type == IMPLEMENTATION) { 
        for_each(i, ast->implementation.body_v) {
          print_AST(ast->implementation.body_v[i]);
          printf(", ");
        } 
        printf("\b\b");
      } else 
        print_AST(ast->variable_definition.expression);
      printf("; ");
      break;
    case F_DEFINITION:
      print_AST(ast->function_definition.declaration);
      printf("\n");
      for_each(i, ast->function_definition.implementations_v)
      print_AST(ast->function_definition.implementations_v + i);
      break;
    case DATA_DECLARATION:
      print_AST(ast->data_declaration.constraints);
      printf("= ");
      for_each(i, ast->data_declaration.decl_v) {
        for_each(j, ast->data_declaration.decl_v[i]) {
          print_AST(ast->data_declaration.decl_v[i] + j);
          printf(" ");
        }
        printf("| ");
      }
      printf("\b\b  ");
      break;
    case FORMAL_TYPE:
      printf("(");
      bool has_constraints = false;
      if (ast->formal_type.constraints_v_v) for_each(i, ast->formal_type.constraints_v_v) {
        for_each(j, ast->formal_type.constraints_v_v[i]) {
          print_AST(ast->formal_type.constraints_v_v[i] + j);
          printf(" ");
        }
        printf("\b, ");
        has_constraints = true;
      }
      printf(has_constraints? "\b\b) => " : ") => ");
      for_each(i, ast->formal_type.type_v) {
        print_AST(ast->formal_type.type_v + i);
        printf(" ");
      }
      break;
    case INSTANCE_DEFINITION:
      printf("INSTANCE_DEFINITION");
      break;
    case CLASS_DECLARATION:
      printf("CLASS_DECLARATION");
      break;
  }
}

static TrieNode* precedence_trie;

static inline unsigned long mkPrecInfo(unsigned int precedence, bool is_infixr) {
  PrecInfo precinfo = (PrecInfo) {.prec = precedence, .is_infixr = is_infixr};
  return *(unsigned long*) &precinfo;
}

static inline PrecInfo get_precedence(char* name, TrieNode* precedence_trie) {
  // TODO: Emit warning when not in trie
  PrecInfo _default = (PrecInfo) { .prec = 6, .is_infixr = false };
  unsigned long res = follow_pattern_with_default(name, precedence_trie, *(unsigned long*) &_default);
  return *(PrecInfo*) &res;
}

static inline bool verify_types(Token** stream, TokenType* types, unsigned long types_l) {
  bool is_correct_type = true;
  for (unsigned long i = 0; i < types_l; i++) {
    is_correct_type &= (*stream)[i].type == types[i];
  }
  *stream += types_l;
  return is_correct_type;
}

static inline TermType tokenT_to_termT(TokenType ttype,  TermType what_is_type) {
  switch (ttype) {
    case NATURAL:     return TNATURAL;
    case REAL:        return TREAL;
    case CHARACTER:   return TCHARACTER;
    case STRING:      return TSTRING;
    case IDENTIFIER:  return FUNCTION;
    case TYPE_K:      return what_is_type;
    default:          return -1;
  }
}

static inline bool in_delims(TokenType type, TokenType* dels, unsigned long dels_s) {
  bool is_in_delims = false;
  for (unsigned long i = 0; i < dels_s; i++)
    is_in_delims |= type == dels[i];
  return is_in_delims;
}

// Returns encoded pointers
static inline ASTNode* parse_predicate(Token** tokens_ptr, bool (*pred)(TokenType), ASTNode* expr, TermType what_is_type) {
  if (!(*pred)((*tokens_ptr)->type))
    return encode_error(*tokens_ptr);
  push(expr, token_to_term(tokenT_to_termT((*tokens_ptr)->type, what_is_type), (**tokens_ptr)));
  (*tokens_ptr)++;
  return expr;
}

// Returns encoded pointers
static inline ASTNode* parse_group(
  Token** tokens_ptr, bool (*is_delim)(TokenType), bool (*is_allowed_term)(TokenType), 
  ASTNode* expr,      TermType what_is_type
) {

  Token* tokens = *tokens_ptr;
  for (; !(*is_delim)(tokens->type); tokens++) {
    if (!(*is_allowed_term)(tokens->type)) {
      return encode_error(tokens);
    }
    push(expr, token_to_term(tokenT_to_termT(tokens->type, what_is_type), (*tokens)));
  }
  *tokens_ptr = tokens;
  return expr;
}

static inline ASTNode* parse_many_identifiers(Token** tokens_ptr, ASTNode* expr) {
  return parse_group(tokens_ptr, not_identifiers, identifiers, expr, 0x0);
}

static inline ASTNode* parse_many_iden_and_types(Token** tokens_ptr, ASTNode* expr) {
  return parse_group(tokens_ptr, not_iden_and_types, iden_and_types, expr, TYPE);
}

// Returns encoded pointers
// call_parser must have closing parenthesis as delimiter
static inline ASTNode* parse_composite_group(
  Token** tokens_ptr, ASTNode* (*group_parser)(Token**, ASTNode*), ASTNode* (*call_parser)(Token**, ASTNode*),
  ASTNode* expr,      TermType what_is_type
) {
parse_rest: {}
  ASTNode* parser_res = (*group_parser)(tokens_ptr, expr);
  if (test_error(parser_res))
    return parser_res;
  if ((*tokens_ptr)->type == OPEN_PAREN) {
    (*tokens_ptr)++;
    ASTNode* nested_expr = Malloc(sizeof(*nested_expr));
    nested_expr->type = EXPRESSION;
    nested_expr->expression_v = new_vector_with_capacity(*nested_expr->expression_v, 4); // maybe change this to be more flexible but idc for now
    (*call_parser)(tokens_ptr, nested_expr->expression_v);
    if ((*tokens_ptr)->type != CLOSE_PAREN)
      return encode_error(*tokens_ptr);
    (*tokens_ptr)++;
    push(expr, *nested_expr);
    goto parse_rest;
  }
  return expr;
}

// Returns encoded pointers
static inline ASTNode* followed_by(
  Token** tokens_ptr, ASTNode* (*first)(Token**, ASTNode*), ASTNode* (*then)(Token**, ASTNode*),
  ASTNode* expr
) {
  ASTNode* first_res = (*first)(tokens_ptr, expr);
  if (test_error(first_res))
    return first_res;
  return (*then)(tokens_ptr, first_res);
}

// Returns encoded pointers
static inline ASTNode* parse_typish(Token** tokens_ptr, ASTNode* constraint) {
  if ((*tokens_ptr)->type != TYPE_K)
    return encode_error(*tokens_ptr);
  push(constraint, token_to_term(TYPE_CONSTRUCTOR, (**tokens_ptr)));
  (*tokens_ptr)++;
  return parse_composite_group(tokens_ptr, &parse_many_identifiers, &parse_typish, constraint, 0x0);
}

// Returns encoded pointers
static inline ASTNode* parse_type_expression(Token** tokens_ptr, ASTNode* data_decl) {
  if ((*tokens_ptr)->type != TYPE_K)
    return encode_error(*tokens_ptr);
  push(data_decl, token_to_term(TYPE_CONSTRUCTOR, (**tokens_ptr)));
  (*tokens_ptr)++;
  return parse_composite_group(tokens_ptr, &parse_many_iden_and_types, &parse_type_expression, data_decl, TYPE);
}

static inline ASTNode* parse_value_group(Token** tokens_ptr, ASTNode* expr) {
  return parse_group(tokens_ptr, non_value, value, expr, TYPE_CONSTRUCTOR);
}

static inline ASTNode* parse_value_expression(Token** tokens_ptr, ASTNode* expr) {
  return parse_composite_group(tokens_ptr, parse_value_group, parse_value_expression, expr, TYPE_CONSTRUCTOR);
}

// Returns encoded pointers
static inline ASTNode* parse_expression(Token** tokens_ptr, ASTNode* expr) {
  ASTNode* root = expr;
  ASTNode* vdef;
  bool is_let = false;
  if ((*tokens_ptr)->type == LET) {
    is_let = true;
    vdef = Malloc(sizeof(*vdef));
    *vdef = (ASTNode) {
      .type = V_DEFINITION,
      .variable_definition.arguments_v = new_vector_with_capacity(*vdef->variable_definition.arguments_v, 4),
      .variable_definition.expression = NULL
    };
    char* name = tokens_ptr[1]->token;
    verify_types(tokens_ptr, identifier, elemsof(identifier));
    if (!verify_types(tokens_ptr, equals, elemsof(equals))) {
      parse_composite_group(tokens_ptr, parse_value_expression, parse_value_expression, vdef->variable_definition.arguments_v, TYPE_CONSTRUCTOR);
      verify_types(tokens_ptr, equals, elemsof(equals));
    }
  }
  parse_composite_group(tokens_ptr, &parse_value_expression, &parse_expression, expr, TYPE_CONSTRUCTOR);
  // TODO: if ... then ... else ...
  if ((*tokens_ptr)->type != OPERATOR) {
    if (is_let) {
      vdef->variable_definition.expression = root;
      return vdef;
    }
    return root;
  }
parse_expr: {}
  PrecInfo curprec = get_precedence((*tokens_ptr)->token, precedence_trie);

  ASTNode* new_bin_op = Malloc(sizeof(*new_bin_op));
  *new_bin_op = token_to_term(FUNCTION, (**tokens_ptr));

  ASTNode* new_bin_expr = Malloc(sizeof(*new_bin_expr));
  new_bin_expr->type = BIN_EXPRESSION;
  new_bin_expr->bin_expression.op = new_bin_op;

  ASTNode* above_node = root;
  ASTNode* prev_above_node;

  bool in_root = true;
  while (
    above_node->type == BIN_EXPRESSION 
    && get_precedence(above_node->bin_expression.op->term.name, precedence_trie).prec 
    + curprec.is_infixr * (curprec.prec == get_precedence(above_node->bin_expression.op->term.name, precedence_trie).prec) 
    > curprec.prec
  ) {
    prev_above_node = above_node;
    above_node = above_node->bin_expression.right_expression_v;
    in_root = false;
  }

  new_bin_expr->bin_expression.left_expression_v = above_node;
  expr = new_vector_with_capacity(*expr, 4);
  new_bin_expr->bin_expression.right_expression_v = expr;

  if (in_root)
    root = new_bin_expr;
  else 
    prev_above_node->bin_expression.right_expression_v = new_bin_expr;

  goto parse_expr;
}

// Returns encoded pointers
static inline ASTNode** parse_sep_by(
  Token** tokens_ptr, ASTNode* (*parse_func)(Token**, ASTNode*), TokenType* delims, unsigned long delims_s,
  unsigned long init_cap
) {
  ASTNode** aggregate_vec = new_vector_with_capacity(*aggregate_vec, init_cap); // NOLINT(bugprone-sizeof-expression)
  (*tokens_ptr)--;
  do {
    (*tokens_ptr)++;
    ASTNode* group_vec = new_vector_with_capacity(*group_vec, init_cap);
    if (test_error((*parse_func)(tokens_ptr, group_vec)))
      return encode_error(*tokens_ptr);
    push(aggregate_vec, group_vec);
  } while (in_delims((*tokens_ptr)->type, delims, delims_s));
  return aggregate_vec;
}

AST parser(Token* tokens, Token** infixes, Error* error_buf) {
  _Static_assert(1 << sizeof(ExpStackIndex) <= MAX_PARENTHESIS  , "ExpStackIndex must be able to entirely index parenthesis stack");
  _Static_assert(sizeof(PrecInfo) <= sizeof(unsigned long)      , "Cannot fit PrecInfo into unsigned long");
  _Static_assert(sizeof(void*) == sizeof(unsigned long)         , "Pointers must be of 64-bits");

  TrieNode* ASTrie = create_node(0, -1);
  TrieNode* type_trie = create_node(0, -1);
  bool is_correct_ast = true;

  precedence_trie = create_node(0, -1);
  insert_trie("+", mkPrecInfo(6, false), precedence_trie);
  insert_trie("*", mkPrecInfo(5, false), precedence_trie);
  // ...
  Token* infix = infixes[0];
  for_each_element(infix, infixes) {
    // blah blah blah errors
    insert_trie(infix[2].token, mkPrecInfo(atoi(infix[1].token), infix[0].type == INFIXR), precedence_trie);
  }

  Token token = tokens[0];
  for_each_element(token, tokens) {
    switch (token.type) {
      case INFIXL:
      case INFIXR:
#ifdef DEBUG
      printf("Infix\n");
#endif /* ifdef DEBUG */
      tokens += 3;
      if (tokens->type != SEMI_COLON) {
        is_correct_ast = false;
        push(
          error_buf,
          mkerr(PARSER, tokens->line, tokens->index, "Missing semicolon")
          // Can't make errors for tokens like these, so find a way
        );
      }
      break;
      case IDENTIFIER: {
#ifdef DEBUG
        printf("Identifier\n");
#endif /* ifdef DEBUG */
        char* name = token.token;
        tokens++;
        if (tokens->type == DOUBLE_COLON) {
          tokens++;
          ASTNode* func_decl = Malloc(sizeof(*func_decl));
          *func_decl = (ASTNode) {
            .type = DECLARATION,
            .declaration.expression_v = NULL,
            .declaration.type_v_v = parse_sep_by(&tokens, parse_type_expression, comas, elemsof(comas), 4)
          };
          ASTNode* ret_type = Malloc(sizeof(*ret_type));
          *ret_type = token_to_term(TYPE, (tokens[1]));
          verify_types(&tokens, arrow_to_type, elemsof(arrow_to_type));
          push(func_decl->declaration.type_v_v, ret_type);
          insert_trie(name, (unsigned long) func_decl, ASTrie);
          verify_types(&tokens, semicolon, elemsof(semicolon));
          break;
        }

        ASTNode* func_impl = Malloc(sizeof(*func_impl));
        *func_impl = (ASTNode) {
          .type = IMPLEMENTATION,
          .implementation.arguments_v = new_vector_with_capacity(*func_impl->implementation.arguments_v, 4),
          .implementation.body_v = new_vector_with_capacity(*func_impl->implementation.arguments_v, 4)
        };
        // Somewhere over here, if theres an equalsm stop too
        // Also, code lets into this
        parse_composite_group(&tokens, parse_value_expression, parse_value_expression, func_impl->implementation.arguments_v, TYPE_CONSTRUCTOR);
        // if stopped == equals dont push expression and in fact start another one
        verify_types(&tokens, equals, elemsof(equals));
        func_impl->implementation.body_v = parse_sep_by(&tokens, parse_expression, comas, elemsof(comas), 4);
        verify_types(&tokens, semicolon, elemsof(semicolon));
        // parse arguments (parse term groups sepby *\{=, ;}), =, parse binary expressions sepby ;
        break;
      }
      case DATA: {
#ifdef DEBUG
        printf("Data\n");
#endif /* ifdef DEBUG */
        ASTNode* data_formal_type = Malloc(sizeof(*data_formal_type));
        data_formal_type->type = FORMAL_TYPE;
        data_formal_type->formal_type.constraints_v_v = NULL;
        tokens++;
        if (tokens->type == OPEN_PAREN) {
          tokens++;
          data_formal_type->formal_type.constraints_v_v = parse_sep_by(&tokens, &parse_typish, comas, elemsof(comas), 4);
          //tokens--;
          verify_types(&tokens, close_constaint, elemsof(close_constaint));
        }
        char* name = tokens->token;
        data_formal_type->formal_type.type_v = new_vector_with_capacity(*data_formal_type->formal_type.type_v, 4);
        parse_typish(&tokens, data_formal_type->formal_type.type_v);
        verify_types(&tokens, equals, elemsof(equals));
        ASTNode* data_declaration = Malloc(sizeof(*data_declaration));
        data_declaration->type = DATA_DECLARATION;
        data_declaration->data_declaration.constraints = data_formal_type;
        data_declaration->data_declaration.decl_v = parse_sep_by(&tokens, parse_type_expression, bars, elemsof(bars), 4);
        tokens++;
        verify_types(&tokens, semicolon, elemsof(semicolon));
        insert_trie(name, (unsigned long) data_declaration, ASTrie);
        break;
      }
      case INSTANCE:
#ifdef DEBUG
        printf("Instance\n");
#endif /* ifdef DEBUG */
        // name (which is constaint)
        // {
        // function implementations
        // }
        break;
      case CLASS:
#ifdef DEBUG
        printf("Class\n");
#endif /* ifdef DEBUG */
        is_correct_ast = false;
        push(
          error_buf,
          mkerr(PARSER, token.line, token.index, "Unsupported, sorry!"));
        break;
        tokens += 3;
      default:
        printf("%d\n", token.type);
        break;
    }
  }

  return (AST) {
    .is_correct_ast = is_correct_ast,
    .astrie = ASTrie,
    .error_buf = error_buf
  };
}
#ifdef DEBUG
#pragma GCC pop_options
#endif

/** TODO:
 * - Have different ties and a function for that checks for redefinitions and inserts in tries
 * - Handle errors in every single instance of functions that return encoded pointers and verify types
 * - (General): Be able to handle errors of missing pre-defined tokens (Perhaps encode the pointer and if it's kernel-space it's just a char?)
 */
