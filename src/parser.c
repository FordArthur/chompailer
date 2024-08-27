#include "parser.h"
#include "scanner.h"
#include "trie.h"
#include "vec.h"

#define elemsof(xs) \
  (sizeof(xs)/sizeof(xs[0]))
#define bool_lambda(name, expr) \
  static inline bool name(TokenType t) { return (expr); }
#define token_to_term(termtype, tok) \
  (ASTNode) {\
    .type = TERM,           .term.type = termtype,      .term.index = tok.index,\
    .term.line = tok.line,  .term.length = tok.length,  .term.name = tok.token\
    }

jmp_buf jumping_buf;

// !! It's really important that the size of this struct isnt greater than unsigned long !!
typedef struct PrecInfo {
  unsigned int prec;
  bool is_infixr;
} PrecInfo;


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
}, value_expression[] = {
  TYPE_K, IDENTIFIER, NATURAL, REAL, CHARACTER, STRING,
}, identifier[] = {
  IDENTIFIER
}, let[] = {
  LET
}, constraint_types[] = {
  TYPE_K, IDENTIFIER
}, open_curly[] = {
  OPEN_CURLY
}, close_curly[] = {
  CLOSE_CURLY
}, arrow[] = {
  ARROW
};

bool_lambda(identifiers, (t == IDENTIFIER));
bool_lambda(not_identifiers, (t != IDENTIFIER));
bool_lambda(iden_and_types, (t == TYPE_K || t == IDENTIFIER));
bool_lambda(not_iden_and_types, (t != TYPE_K && t != IDENTIFIER));
bool_lambda(value, (in_delims(t, value_expression, elemsof(value_expression))))
bool_lambda(value_delims, (t == COMA || t == SEMI_COLON || t == EQUALS || t == OPERATOR || t == OPEN_PAREN || t == CLOSE_PAREN));

static bool top_level_assertion = true;
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
    case TYPE_ASSERTION:
      if (!top_level_assertion) for_each(i, ast->type_assertion.expression) {
        print_AST(ast->type_assertion.expression + i);
        printf(" ");
      } else {
        printf("%s ", ast->type_assertion.expression->term.name);
      }
      printf(":: ");
      printf("(");
      if (ast->type_assertion.constraints) for_each(i, ast->type_assertion.constraints) {
        printf("%s is ", ast->type_assertion.constraints[i][0].term.name);
        for_each(j, ast->type_assertion.constraints[i][1].expression_v) {
          printf("%s, ", ast->type_assertion.constraints[i][1].expression_v[j].term.name);
        }
        printf("\b\b");
      }
      printf(") => ");
      for (unsigned long i = 0; i < _get_header(ast->type_assertion.type_v)->size; i++) {
        print_AST(ast->type_assertion.type_v + i);
        printf(", ");
      }
      printf("\b\b  ");
      break;
    case V_DEFINITION:
      printf("let %s = ", ast->variable_definition.name->term.name);
      print_AST(ast->variable_definition.expression);
      break;
    case IMPLEMENTATION:
      for_each(i, ast->implementation.lhs) {
        print_AST(ast->implementation.lhs + i);
        printf(" ");
      }
      printf("= ");
      top_level_assertion = false;
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
      printf("\b; ");
      top_level_assertion = true;
      break;
    case DATA_DECLARATION:
      printf("data ");
      for_each(i, ast->data_declaration.type) {
        printf("%s, ", ast->data_declaration.type[i].term.name);
      }
      printf("\b\b = ");
      for_each(i, ast->data_declaration.constructors) {
        for_each(j, ast->data_declaration.constructors[i]) {
          printf("%s ", ast->data_declaration.constructors[i][j].term.name);
        }
        printf("| ");
      }
      break;
    case INSTANCE_DEFINITION:
      printf("instance %s for %s, where\n", ast->instance_definition.instance_class->term.name, ast->instance_definition.instance_type->term.name);
      for_each(i, ast->instance_definition.implementations_v) {
        printf("\t");
        print_AST(ast->instance_definition.implementations_v[i]);
        printf("\n");
      }
      break;
    case CLASS_DECLARATION:
      printf("class %s, where", ast->class_declaration.class_name->term.name);
      for_each(i, ast->class_declaration.declarations_v) {
        printf("\t");
        print_AST(ast->class_declaration.declarations_v[i]);
        printf("\n");
      }
      break;
  }
}

static TrieNode* precedence_trie;
static Error* error_buf;
static bool is_correct_ast = true;
static TokenType skip_to_tok;

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

static inline void handle(Token** tokens_ptr, char* err_msg) {
  is_correct_ast = false;
  push(error_buf, mkerr(PARSER, (*tokens_ptr)->line, (*tokens_ptr)->index, err_msg));
  for (; (*tokens_ptr)->type != skip_to_tok && (*tokens_ptr)->type != EndOfFile; (*tokens_ptr)++);
  if ((*tokens_ptr)->type != EndOfFile) (*tokens_ptr)++;
  longjmp(jumping_buf, 0);
}

static inline bool verify_types(Token** stream, TokenType* types, unsigned long types_l) {
  for (unsigned long i = 0; i < types_l; i++, (*stream)++) {
    if ((*stream)[0].type != types[i])
      return false;
  }
  return true;
}

static inline TermType tokenT_to_termT(TokenType ttype) {
  switch (ttype) {
    case NATURAL:     return TNATURAL;
    case REAL:        return TREAL;
    case CHARACTER:   return TCHARACTER;
    case STRING:      return TSTRING;
    case IDENTIFIER:  return FUNCTION;
    case TYPE_K:      return TYPE_CONSTRUCTOR;
    default:          return -1;
  }
}

static inline bool in_delims(TokenType type, TokenType* dels, unsigned long dels_s) {
  bool is_in_delims = false;
  for (unsigned long i = 0; i < dels_s; i++)
    is_in_delims |= type == dels[i];
  return is_in_delims;
}

// Handles failures
static inline ASTNode* parse_group(
  Token** tokens_ptr, bool (*is_delim)(TokenType), bool (*is_allowed_term)(TokenType), 
  ASTNode* expr,      TermType what_is_type
) {
  Token* tokens = *tokens_ptr;
  for (; !(*is_delim)(tokens->type); tokens++) {
    if (!(*is_allowed_term)(tokens->type))
      handle(&tokens, "Unexpected token");

    push(expr, token_to_term(tokenT_to_termT(tokens->type), (*tokens)));
  }
  *tokens_ptr = tokens;
  return expr;
}

static inline ASTNode* parse_many_identifiers(Token** tokens_ptr, ASTNode* expr) {
  return parse_group(tokens_ptr, not_identifiers, identifiers, expr, 0x0);
}

static inline ASTNode* parse_many_iden_and_types(Token** tokens_ptr, ASTNode* expr) {
  return parse_group(tokens_ptr, not_iden_and_types, iden_and_types, expr, TYPE_CONSTRUCTOR);
}

// call_parser must have closing parenthesis as delimiter
static inline ASTNode* parse_composite_group(
  Token** tokens_ptr, ASTNode* (*group_parser)(Token**, ASTNode*), ASTNode* (*call_parser)(Token**, ASTNode*),
  ASTNode* expr,      TermType what_is_type
) {
parse_rest: {}
  (*group_parser)(tokens_ptr, expr);
  if ((*tokens_ptr)->type == OPEN_PAREN) {
    (*tokens_ptr)++;
    ASTNode* nested_expr = new_vector_with_capacity(*nested_expr->expression_v, 4);
    ASTNode* nested_node = (*call_parser)(tokens_ptr, nested_expr);
    if ((*tokens_ptr)->type != CLOSE_PAREN)
      handle(tokens_ptr, "Unmatched opening parenthesis");
    (*tokens_ptr)++;
    push(expr, *nested_node);
    goto parse_rest;
  }
  return expr;
}

static inline ASTNode* parse_typish(Token** tokens_ptr, ASTNode* constraint) {
  if ((*tokens_ptr)->type != TYPE_K)
    handle(tokens_ptr, "Expected type");
  push(constraint, token_to_term(TYPE_CONSTRUCTOR, (**tokens_ptr)));
  (*tokens_ptr)++;
  return parse_composite_group(tokens_ptr, &parse_many_identifiers, &parse_typish, constraint, 0x0);
}

static inline ASTNode* parse_type_expression(Token** tokens_ptr, ASTNode* data_decl) {
  if ((*tokens_ptr)->type == IDENTIFIER) {
    push(data_decl, token_to_term(TYPE_CONSTRUCTOR, (**tokens_ptr)));
    (*tokens_ptr)++;
    return data_decl;
  }
  if ((*tokens_ptr)->type != TYPE_K)
    handle(tokens_ptr, "Expected type");
  push(data_decl, token_to_term(TYPE_CONSTRUCTOR, (**tokens_ptr)));
  (*tokens_ptr)++;
  return parse_composite_group(tokens_ptr, &parse_many_iden_and_types, &parse_type_expression, data_decl, TYPE_CONSTRUCTOR);
}

static inline ASTNode* parse_value_group(Token** tokens_ptr, ASTNode* expr) {
  return parse_group(tokens_ptr, value_delims, value, expr, TYPE_CONSTRUCTOR);
}

static inline ASTNode* parse_value_expression(Token** tokens_ptr, ASTNode* expr) {
  return parse_composite_group(tokens_ptr, parse_value_group, parse_value_expression, expr, TYPE_CONSTRUCTOR);
}

static inline ASTNode* parse_left_hand_side_bind(Token** tokens_ptr) {
  ASTNode* lhs = new_vector_with_capacity(*lhs, 4);
  push(lhs, token_to_term(FUNCTION, (**tokens_ptr)));
  (*tokens_ptr)++;
  parse_value_group(tokens_ptr, lhs);
  return lhs;
}

static inline ASTNode* parse_expression(Token** tokens_ptr, ASTNode* expr) {
  ASTNode* root = expr;
  ASTNode* vdef;
  bool is_let = false;
  if (verify_types(tokens_ptr, let, elemsof(let))) {
    is_let = true;
    if(!verify_types(tokens_ptr, identifier, elemsof(identifier)))
      handle(tokens_ptr, "Must bind to a name");
    if (!verify_types(tokens_ptr, equals, elemsof(equals)))
      handle(tokens_ptr, "Expected equals");
    vdef = Malloc(2*sizeof(*vdef));
    vdef[1] = token_to_term(FUNCTION, (*tokens_ptr)[-2]);
    *vdef = (ASTNode) {
      .type = V_DEFINITION,
      .variable_definition.name = vdef + 1
    };
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
    group_vec = (*parse_func)(tokens_ptr, group_vec);
    push(aggregate_vec, group_vec);
  } while (in_delims((*tokens_ptr)->type, delims, delims_s));
  return aggregate_vec;
}

static inline ASTNode** parse_constraint(Token** tokens_ptr) {
  // TODO: This will skip a constraint if its empty (`example :: (Constraint a, C) => ...` wont throw an error and catch `C`), fix later but idc for now
  if ((*tokens_ptr)->type != OPEN_PAREN) return NULL;
  (*tokens_ptr)++;
  ASTNode** constraints = new_vector_with_capacity(*constraints, 4); // NOLINT(bugprone-sizeof-expression)
  
add_constraint:
  if (!verify_types(tokens_ptr, constraint_types, elemsof(constraint_types)))
    handle(tokens_ptr, "Unexpected token");

  ASTNode* constraint = constraints[0];
  bool was_inserted = false;
  for_each_element(constraint, constraints) {
    if (strcmp(constraint->term.name, (*tokens_ptr)[-1].token) == 0) {
      push(constraint[1].expression_v, token_to_term(TYPE_CONSTRUCTOR, (*tokens_ptr)[-2]));
      was_inserted = true;
      break;
    }
  }

  if (!was_inserted) {
    ASTNode* new_constraints = new_vector_with_capacity(*new_constraints, 2);
    push(new_constraints, token_to_term(TYPE_CONSTRUCTOR, (*tokens_ptr)[-2]));
    ASTNode* new_var = Malloc(sizeof(ASTNode)*2);
    new_var[0] = token_to_term(TYPE_CONSTRUCTOR, (*tokens_ptr)[-1]);
    new_var[1] = (ASTNode) { .expression_v = new_constraints };

    push(constraints, new_var);
  } 

  if (verify_types(tokens_ptr, comas, elemsof(comas)))
    goto add_constraint;

  if (!verify_types(tokens_ptr, close_constaint, elemsof(close_constaint)))
    handle(tokens_ptr, "Expected closing constraint sequence");

  return constraints;
}

static inline ASTNode* parse_implementation(Token** tokens_ptr, ASTNode* ast) {
  ASTNode* name = Malloc(sizeof(*name));
  *name = token_to_term(FUNCTION, (**tokens_ptr));
  ASTNode* left_hand_side = parse_left_hand_side_bind(tokens_ptr);
  ASTNode** body = parse_sep_by(tokens_ptr, parse_value_group, comas, elemsof(comas), 4);
  if (!verify_types(tokens_ptr, semicolon, elemsof(semicolon)))
    handle(tokens_ptr, "Expected semicolon");
  push(ast, ((ASTNode) { .type = IMPLEMENTATION, .implementation.lhs = left_hand_side, .implementation.body_v = body } ) );
  return ast;
}

static inline ASTNode* flatten_type(ASTNode** type_v_v) {
  ASTNode* type_v = new_vector_with_capacity(*type_v, sizeof_vector(type_v_v));
  for_each(i, type_v_v) {
    ASTNode type;
    if (sizeof_vector(type_v_v[i]) == 1)
        type = type_v_v[i][0];
    else
      type = (ASTNode) { .type = EXPRESSION, .expression_v = type_v_v[i] };
    push(type_v, type);
  }
  return type_v;
}

static inline ASTNode* parse_declaration(Token** tokens_ptr, ASTNode* ast) {
  ASTNode* name = Malloc(sizeof(*name));
  *name = token_to_term(FUNCTION, (**tokens_ptr));
  ASTNode** constraints = parse_constraint(tokens_ptr);
  if (!verify_types(tokens_ptr, close_constaint, elemsof(close_constaint)))
    handle(tokens_ptr, "Expected closing constraint sequence");
  ASTNode** type_v = parse_sep_by(tokens_ptr, &parse_type_expression, comas, elemsof(comas), 4);
  ASTNode* ret_type = new_vector_with_capacity(*ret_type, 4);
  push(type_v, ret_type);
  parse_type_expression(tokens_ptr, vector_last(type_v));
  if (!verify_types(tokens_ptr, semicolon, elemsof(semicolon)))
    handle(tokens_ptr, "Expected semicolon");
  push(ast, ((ASTNode) { .type = TYPE_ASSERTION, .type_assertion.expression = name, .type_assertion.constraints = constraints, .type_assertion.type_v = flatten_type(type_v) }));
  return ast;
}

AST parser(Token* tokens, Token** infixes, Error* _error_buf) {
  _Static_assert(sizeof(PrecInfo) <= sizeof(unsigned long)      , "Cannot fit PrecInfo into unsigned long");
  _Static_assert(sizeof(void*) == sizeof(unsigned long)         , "Pointers must be of 64-bits");

  ASTNode* ast = new_vector_with_capacity(*ast, 256);
  error_buf = _error_buf;
  precedence_trie = create_node(0, -1);

  Token* infix = infixes[0];
  for_each_element(infix, infixes) {
    // TODO: Add errors 
    insert_trie(infix[2].token, mkPrecInfo(atoi(infix[1].token), infix[0].type == INFIXR), precedence_trie);
  }

  for (Token token = tokens[0]; token.type != EndOfFile; token = tokens[0]) {
    setjmp(jumping_buf);
    switch (token.type) {
      case INFIXL:
      case INFIXR:
        tokens += 4;
        break;
      case LET:
        skip_to_tok = SEMI_COLON;
        push(ast, *parse_value_expression(&tokens, new_vector_with_capacity(ASTNode, 4)));
        break;
      case OPERATOR:
      case IDENTIFIER: {
        skip_to_tok = SEMI_COLON;
        ASTNode* name = Malloc(sizeof(*name));
        *name = token_to_term(FUNCTION, (*tokens));
        if (tokens[1].type == DOUBLE_COLON) {
          tokens += 2;
          ASTNode** constraints = parse_constraint(&tokens);
          // TODO: check that at least one was parsed
          ASTNode** type_v = parse_sep_by(&tokens, &parse_type_expression, comas, elemsof(comas), 4);
          if (!verify_types(&tokens, arrow, elemsof(arrow)))
            handle(&tokens, "Expected arrow");
          ASTNode* ret_type = new_vector_with_capacity(*ret_type, 4);
          parse_type_expression(&tokens, ret_type);
          push(type_v, ret_type);
          if (!verify_types(&tokens, semicolon, elemsof(semicolon)))
            handle(&tokens, "Expected semicolon");
          push(ast, ((ASTNode) { .type = TYPE_ASSERTION, .type_assertion.expression = name, .type_assertion.constraints = constraints, .type_assertion.type_v = flatten_type(type_v) }));
          break;
        }
        ASTNode* left_hand_side = parse_left_hand_side_bind(&tokens);
        if (!verify_types(&tokens, equals, elemsof(equals)))
          handle(&tokens, "Expected equals");
        ASTNode** body = parse_sep_by(&tokens, &parse_expression, comas, elemsof(comas), 4);
        if (!verify_types(&tokens, semicolon, elemsof(semicolon)))
          handle(&tokens, "Expected semicolon");
        push(ast, ((ASTNode) { .type = IMPLEMENTATION, .implementation.lhs = left_hand_side, .implementation.body_v = body } ) );
        break;
      }
      case DATA: {
        skip_to_tok = SEMI_COLON;
        tokens++;
        ASTNode* type = new_vector_with_capacity(*type, 4);
        parse_typish(&tokens, type);
        verify_types(&tokens, equals, elemsof(equals));
        ASTNode** constructors = parse_sep_by(&tokens, parse_type_expression, bars, elemsof(bars), 4);
        push(ast, ((ASTNode) { .type = DATA_DECLARATION, .data_declaration.type = type, .data_declaration.constructors = constructors } ));
        if (!verify_types(&tokens, semicolon, elemsof(semicolon)))
          handle(&tokens, "Expected semicolon");
        break;
      }
      case INSTANCE: {
        skip_to_tok = CLOSE_CURLY;
        tokens++;
        if (tokens->type != TYPE_K)
          handle(&tokens, "Expected class");
        ASTNode* instance_info = Malloc(sizeof(ASTNode)*2);
        instance_info[0] = token_to_term(TYPE_CONSTRUCTOR, (*tokens));
        tokens++;
        if (tokens->type != TYPE_K)
          handle(&tokens, "Expected type");
        instance_info[1] = token_to_term(TYPE_CONSTRUCTOR, (*tokens));
        if (verify_types(&tokens, open_curly, elemsof(open_curly)))
          handle(&tokens, "Expected opening curly brace");
        ASTNode** implementations = parse_sep_by(&tokens, parse_implementation, semicolon, elemsof(semicolon), 4);
        if (verify_types(&tokens, close_curly, elemsof(close_curly)))
          handle(&tokens, "Expected opening curly brace");
        push(ast, ((ASTNode) { .type = INSTANCE_DEFINITION, .instance_definition.instance_class = instance_info, .instance_definition.instance_type = instance_info + 1, .instance_definition.implementations_v = implementations }));
        break;
      }
      case CLASS: {
        skip_to_tok = CLOSE_CURLY;
        tokens++;
        if (tokens->type != TYPE_K)
          handle(&tokens, "Expected class");
        ASTNode* name = Malloc(sizeof(ASTNode));
        *name = token_to_term(FUNCTION, (*tokens));
        if (verify_types(&tokens, open_curly, elemsof(open_curly)))
          handle(&tokens, "Expected opening curly brace");
        ASTNode** declarations = parse_sep_by(&tokens, parse_declaration, semicolon, elemsof(semicolon), 4);
        if (verify_types(&tokens, close_curly, elemsof(close_curly)))
          handle(&tokens, "Expected opening curly brace");
        push(ast, ((ASTNode) { .type = CLASS_DECLARATION, .class_declaration.class_name = name, .class_declaration.declarations_v = declarations }))
      }
      default:
        printf("%d\n", token.type);
        tokens++;
        break;
    }
  }

  return (AST) {
    .is_correct_ast = is_correct_ast,
    .ast = ast,
    .error_buf = error_buf
  };
}

#ifdef DEBUG
#pragma GCC pop_options
#endif

/** TODO:
 * - If then else, vector/tuple literals
 * - Upgrade handle to take a sequence of tokens sorted by preference on when to stop
 */
