#include "parser.h"

#define elemsof(xs) \
  (sizeof(xs)/sizeof(xs[0]))
#define bool_lambda(name, expr) \
  static inline bool name(TokenType t) { return (expr); }
#define token_to_term(termtype, tok) \
  (ASTNode) {\
    .type = TERM,           .term.type = termtype,      .term.index = tok.index,\
    .term.line = tok.line,  .term.length = tok.length,  .term.name = tok.token\
    }

// !! It's really important that the size of this struct isnt greater than unsigned long !!
typedef struct PrecInfo {
  unsigned int prec;
  bool is_infixr;
} PrecInfo;

typedef unsigned char ExpStackIndex;

#ifdef DEBUG
#pragma GCC push_options
#pragma GCC optimize ("O0")
#endif

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
}, arrow[] = {
  ARROW
}, value_expression[] = {
  TYPE_K, IDENTIFIER, NATURAL, REAL, CHARACTER, STRING,
}, identifier[] = {
  IDENTIFIER
}, let[] = {
  LET
}, constraint_types[] = {
  TYPE_K, IDENTIFIER
};

bool_lambda(identifiers, (t == IDENTIFIER));
bool_lambda(not_identifiers, (t != IDENTIFIER));
bool_lambda(iden_and_types, (t == TYPE_K || t == IDENTIFIER));
bool_lambda(not_iden_and_types, (t != TYPE_K && t != IDENTIFIER));
bool_lambda(value, (in_delims(t, value_expression, elemsof(value_expression))))
bool_lambda(value_delims, (t == COMA || t == SEMI_COLON || t == EQUALS || t == OPERATOR || t == OPEN_PAREN || t == CLOSE_PAREN));

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
      for_each(i, ast->bin_expression.left_expression_v) {
        print_AST(ast->bin_expression.left_expression_v + i);
        printf(" ");
      }
      printf("\b) (");
      for_each(i, ast->bin_expression.right_expression_v) {
        print_AST(ast->bin_expression.right_expression_v + i);
        printf(" ");
      }
      printf("\b)");
      break;
    case DECLARATION:
      if (ast->declaration.expression_v) for_each(i, ast->declaration.expression_v) {
        print_AST(ast->declaration.expression_v + i);
        printf(" ");
      }
      printf(":: ");
      print_AST(ast->declaration.formal_type);
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
          if (ast->implementation.body_v[i]->type == BIN_EXPRESSION
              || ast->implementation.body_v[i]->type == V_DEFINITION)
            print_AST(ast->implementation.body_v[i]);
          else for_each(j, ast->implementation.body_v[i]) {
            print_AST(ast->implementation.body_v[i] + j);
            printf(" ");
          }
          printf(", ");
        } 
        printf("\b\b");
      } else if (ast->variable_definition.expression->type == BIN_EXPRESSION)
        print_AST(ast->variable_definition.expression);
      else for_each(i, ast->variable_definition.expression)
        print_AST(ast->variable_definition.expression + i);
      printf("; ");
      break;
    case F_DEFINITION:
      print_AST(ast->function_definition.declaration);
      printf("\n");
      for_each(i, ast->function_definition.implementations_v)
      print_AST(ast->function_definition.implementations_v + i);
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
      for (unsigned long i = 0; i < _get_header(ast->formal_type.type_v_v)->size; i++) {
        for (unsigned long j = 0; j < _get_header(ast->formal_type.type_v_v[i])->size; j++) {
          print_AST(ast->formal_type.type_v_v[i] + j);
          printf(" ");
        }
        printf(", ");
      }
      printf("\b");
      break;
    case CONSTRUCTOR_DEFINITION:
      printf("CONSTRUCTOR_DEFINITION");
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

static inline bool handle(
  bool ptr,           bool* err_mark, Error* err_buf,
  Token** tokens_ptr, char* err_msg,  TokenType skip_to_tok
) {
  if (!ptr) {
    *err_mark = false;
    push(err_buf, mkerr(PARSER, (*tokens_ptr)->line, (*tokens_ptr)->index, err_msg));
    for (; (*tokens_ptr)->type != skip_to_tok && (*tokens_ptr)->type != EndOfFile; (*tokens_ptr)++);
    (*tokens_ptr)++;
    return true;
  }
  return false;
}

static inline bool verify_types(Token** stream, TokenType* types, unsigned long types_l) {
  for (unsigned long i = 0; i < types_l; i++, (*stream)++) {
    if ((*stream)[0].type != types[i])
      return false;
  }
  return true;
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

static inline ASTNode* parse_group(
  Token** tokens_ptr, bool (*is_delim)(TokenType), bool (*is_allowed_term)(TokenType), 
  ASTNode* expr,      TermType what_is_type
) {

  Token* tokens = *tokens_ptr;
  for (; !(*is_delim)(tokens->type); tokens++) {
    if (!(*is_allowed_term)(tokens->type)) {
      return NULL;
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

// call_parser must have closing parenthesis as delimiter
static inline ASTNode* parse_composite_group(
  Token** tokens_ptr, ASTNode* (*group_parser)(Token**, ASTNode*), ASTNode* (*call_parser)(Token**, ASTNode*),
  ASTNode* expr,      TermType what_is_type
) {
parse_rest: {}
  ASTNode* parser_res = (*group_parser)(tokens_ptr, expr);
  if (!parser_res)
    return NULL;
  if ((*tokens_ptr)->type == OPEN_PAREN) {
    (*tokens_ptr)++;
    ASTNode* nested_expr = new_vector_with_capacity(*nested_expr->expression_v, 4);
    ASTNode* nested_node = (*call_parser)(tokens_ptr, nested_expr);
    if ((*tokens_ptr)->type != CLOSE_PAREN)
      return NULL;
    (*tokens_ptr)++;
    push(expr, *nested_node);
    goto parse_rest;
  }
  return expr;
}

static inline ASTNode* parse_typish(Token** tokens_ptr, ASTNode* constraint) {
  if ((*tokens_ptr)->type != TYPE_K)
    return NULL;
  push(constraint, token_to_term(TYPE_CONSTRUCTOR, (**tokens_ptr)));
  (*tokens_ptr)++;
  return parse_composite_group(tokens_ptr, &parse_many_identifiers, &parse_typish, constraint, 0x0);
}

static inline ASTNode* parse_type_expression(Token** tokens_ptr, ASTNode* data_decl) {
  if ((*tokens_ptr)->type == IDENTIFIER) {
    push(data_decl, token_to_term(TYPE, (**tokens_ptr)));
    (*tokens_ptr)++;
    return data_decl;
  }
  if ((*tokens_ptr)->type != TYPE_K)
    return NULL;
  push(data_decl, token_to_term(TYPE_CONSTRUCTOR, (**tokens_ptr)));
  (*tokens_ptr)++;
  return parse_composite_group(tokens_ptr, &parse_many_iden_and_types, &parse_type_expression, data_decl, TYPE);
}

static inline ASTNode* parse_value_group(Token** tokens_ptr, ASTNode* expr) {
  return parse_group(tokens_ptr, value_delims, value, expr, TYPE_CONSTRUCTOR);
}

static inline ASTNode* parse_value_expression(Token** tokens_ptr, ASTNode* expr) {
  return parse_composite_group(tokens_ptr, parse_value_group, parse_value_expression, expr, TYPE_CONSTRUCTOR);
}

static inline ASTNode* parse_expression(Token** tokens_ptr, ASTNode* expr) {
  ASTNode* root = expr;
  ASTNode* vdef;
  bool is_let = false;
  if (verify_types(tokens_ptr, let, elemsof(let))) {
    is_let = true;
    vdef = Malloc(sizeof(*vdef));
    *vdef = (ASTNode) {
      .type = V_DEFINITION,
      .variable_definition.arguments_v = new_vector_with_capacity(*vdef->variable_definition.arguments_v, 4),
    };
    if(verify_types(tokens_ptr, identifier, elemsof(identifier)))
      return NULL;
    if (!verify_types(tokens_ptr, equals, elemsof(equals))) {
      parse_value_expression(tokens_ptr, vdef->variable_definition.arguments_v);
      if(verify_types(tokens_ptr, equals, elemsof(equals)))
        return NULL;
    }
  }
parse_expr:

  parse_composite_group(tokens_ptr, parse_value_group, parse_expression, expr, TYPE_CONSTRUCTOR);
  // TODO: if ... then ... else ...
  //     : type assertions (::)
  if ((*tokens_ptr)->type != OPERATOR) {
    if (is_let) {
      vdef->variable_definition.expression = root;
      return vdef;
    }
    return root;
  }
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

  (*tokens_ptr)++;
  goto parse_expr;
}

static inline ASTNode** parse_sep_by(
  Token** tokens_ptr, ASTNode* (*parse_func)(Token**, ASTNode*), TokenType* delims, unsigned long delims_s,
  unsigned long init_cap
) {
  ASTNode** aggregate_vec = new_vector_with_capacity(*aggregate_vec, init_cap); // NOLINT(bugprone-sizeof-expression)
  (*tokens_ptr)--;
  do {
    (*tokens_ptr)++;
    ASTNode* group_vec = new_vector_with_capacity(*group_vec, init_cap);
    if (!(group_vec = (*parse_func)(tokens_ptr, group_vec)))
      return NULL;
    push(aggregate_vec, group_vec);
  } while (in_delims((*tokens_ptr)->type, delims, delims_s));
  return aggregate_vec;
}

static inline ASTNode* parse_constraint(Token** tokens_ptr, ASTNode* constraint) {
  verify_types(tokens_ptr, constraint_types, elemsof(constraint_types));
  push(constraint, token_to_term(TYPE_CONSTRUCTOR, (*tokens_ptr)[-2]));
  push(constraint, token_to_term(TYPE_CONSTRUCTOR, (*tokens_ptr)[-1]));
  return constraint;
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

  for (Token token = tokens[0]; token.type != EndOfFile; token = tokens[0]) {
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
      case OPERATOR:
      case IDENTIFIER: {
#ifdef DEBUG
        printf("Identifier\n");
#endif /* ifdef DEBUG */
        char* name = token.token;
        tokens++;
        if (tokens->type == DOUBLE_COLON) {
          tokens++;
          ASTNode* formal_type = Malloc(sizeof(*formal_type));
          *formal_type = (ASTNode) {
            .type = FORMAL_TYPE,
            .formal_type.constraints_v_v = NULL,
          };
          ASTNode* func_decl = Malloc(sizeof(*func_decl));
          *func_decl = (ASTNode) {
            .type = DECLARATION,
            .declaration.expression_v = NULL,
            .declaration.formal_type = formal_type
          };
          if (tokens->type == OPEN_PAREN) {
            tokens++;
            formal_type->formal_type.constraints_v_v = parse_sep_by(&tokens, &parse_typish, comas, elemsof(comas), 4);
            if (
              handle(formal_type->formal_type.constraints_v_v && verify_types(&tokens, close_constaint, elemsof(close_constaint)),
                     &is_correct_ast, error_buf, &tokens, "Unexpected token", SEMI_COLON)
            ) break;
          }
          ASTNode* ret_type = new_vector_with_capacity(*ret_type, 4);
          formal_type->formal_type.type_v_v = parse_sep_by(&tokens, parse_type_expression, comas, elemsof(comas), 4);
          if (
            handle(formal_type->formal_type.type_v_v && verify_types(&tokens, arrow, elemsof(arrow)),
                   &is_correct_ast, error_buf, &tokens, "Unexpected token", SEMI_COLON)
          ) break;
          parse_type_expression(&tokens, ret_type);
          push(formal_type->formal_type.type_v_v, ret_type);
          if (
            handle(verify_types(&tokens, semicolon, elemsof(semicolon)), &is_correct_ast,
                   error_buf, &tokens, "Missing semicolon", SEMI_COLON)
          ) break;
          ASTNode* def;
          if ((def = (ASTNode*)follow_pattern_with_default(name, ASTrie, 0))) {
            is_correct_ast = false;
            push(error_buf, mkerr(PARSER, tokens->line, tokens->index, "Cannot redefine type"))
          } else {
            if (!def) {
              def = Malloc(sizeof(*def));
              *def = (ASTNode) {
                .type = F_DEFINITION,
                .function_definition.implementations_v = new_vector_with_capacity(*def->function_definition.implementations_v, 4),
              };
              insert_trie(name, (unsigned long) def, ASTrie);
            }
            def->function_definition.declaration = func_decl;
          }
          break;
        }

        ASTNode* func_impl = Malloc(sizeof(*func_impl));
        *func_impl = (ASTNode) {
          .type = IMPLEMENTATION,
          .implementation.arguments_v = new_vector_with_capacity(*func_impl->implementation.arguments_v, 4),
        };
        if (
          handle(parse_value_expression(&tokens, func_impl->implementation.arguments_v),
                 &is_correct_ast, error_buf, &tokens, "Unexpected token", SEMI_COLON)
        ) break;
        if (
          handle(verify_types(&tokens, equals, elemsof(equals)), &is_correct_ast,
                 error_buf, &tokens, "Expected equals", SEMI_COLON)
        ) break;
        func_impl->implementation.body_v = parse_sep_by(&tokens, parse_expression, comas, elemsof(comas), 4);
        if (handle(func_impl->implementation.body_v, &is_correct_ast, error_buf, &tokens, "Unexpected token", SEMI_COLON))
          break;
        if (!verify_types(&tokens, semicolon, elemsof(semicolon))) {
          is_correct_ast = false;
          push(error_buf, mkerr(PARSER, tokens->line, tokens->index, "Missing semicolon"))
        }
        ASTNode* def;
        if ((def = (ASTNode*)follow_pattern_with_default(name, ASTrie, 0))) {
          push(def->function_definition.implementations_v, *func_impl);
        } else {
          def = Malloc(sizeof(*def));
          *def = (ASTNode) {
            .type = F_DEFINITION,
            .function_definition.implementations_v = new_vector_with_capacity(*def->function_definition.implementations_v, 4)
          };
          push(def->function_definition.implementations_v, *func_impl);
          insert_trie(name, (unsigned long) def, ASTrie);
        }
        break;
      }
      case DATA: {
#ifdef DEBUG
        printf("Data\n");
#endif /* ifdef DEBUG */
        tokens++;
        ASTNode** constraints;
        if (tokens->type == OPEN_PAREN) {
          tokens++;
          constraints = parse_sep_by(&tokens, &parse_typish, comas, elemsof(comas), 4);
          if (
            handle(constraints && verify_types(&tokens, close_constaint, elemsof(close_constaint)),
                   &is_correct_ast, error_buf, &tokens, "Unexpected token", SEMI_COLON)
          ) break;
        }
        char* name = tokens->token;
        ASTNode* constructs = new_vector_with_capacity(*constructs, 4);
        if (handle(parse_typish(&tokens, constructs), &is_correct_ast, error_buf, &tokens, "Unexpected token", SEMI_COLON))
            break;
        if (!verify_types(&tokens, equals, elemsof(equals))) {
          is_correct_ast = false;
          push(error_buf, mkerr(PARSER, tokens->line, tokens->index, "Expected equals"));
          tokens++;
          break;
        }
        ASTNode** constructors = parse_sep_by(&tokens, parse_type_expression, bars, elemsof(bars), 4);
        if (handle(constructors, &is_correct_ast, error_buf, &tokens, "Unexpected token", SEMI_COLON))
          break;
        if (!verify_types(&tokens, semicolon, elemsof(semicolon))) {
          is_correct_ast = false;
          push(error_buf, mkerr(PARSER, tokens->line, tokens->index, "Missing semicolon"));
        }
        ASTNode* def;
        if ((def = (ASTNode*)follow_pattern_with_default(name, ASTrie, 0))) {
          is_correct_ast = false;
          push(error_buf, mkerr(PARSER, tokens->line, tokens->index, "Cannot redefine data declaration"));
          break;
        } 
        ASTNode* constructor = constructors[0];
        for_each_element(constructor, constructors) {
          push(constructor, *constructs);
          ASTNode* constructor_def = Malloc(sizeof(*constructor_def));
          *constructor_def = (ASTNode) {
            .type = CONSTRUCTOR_DEFINITION,
            .constructor_definition.constraints_v_v = constraints,
            .constructor_definition.constructor_type = constructor
          };
          insert_trie(constructor[0].term.name, (unsigned long) constructor_def, ASTrie);
        }
        break;
      }
      case INSTANCE:
#ifdef DEBUG
        printf("Instance\n");
#endif /* ifdef DEBUG */
        handle(false, &is_correct_ast, error_buf, &tokens, "Not supported, sorry!", CLOSE_CURLY);
      case CLASS:
#ifdef DEBUG
        printf("Class\n");
#endif /* ifdef DEBUG */
        handle(false, &is_correct_ast, error_buf, &tokens, "Not supported, sorry!", CLOSE_CURLY);
        break;
      default:
        printf("%d\n", token.type);
        tokens++;
        break;
    }
  }

  return (AST) {
    .is_correct_ast = is_correct_ast,
    .astrie = ASTrie,
    .type_trie = type_trie,
    .error_buf = error_buf
  };
}

#ifdef DEBUG
#pragma GCC pop_options
#endif

/** TODO:
 * - Handle errors in every single instance of functions that return encoded pointers and verify types
 * - If then else, vector/tuple literals
 * - Error when multiple values of constraints
 * - Sort constraints
 */
