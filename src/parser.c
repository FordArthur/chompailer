#include "parser.h"
#include "vec.h"
#include <string.h>

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
}, type_k[] = {
  TYPE_K
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
      if (ast->formal_type.constraints_v_v) for_each(i, ast->formal_type.constraints_v_v) {
        printf("%s is ", ast->formal_type.constraints_v_v[i][0].term.name);
        for_each(j, ast->formal_type.constraints_v_v[i][1].expression_v) {
          printf("%s, ", ast->formal_type.constraints_v_v[i][1].expression_v[j].term.name);
        }
        printf("\b\b");
      }
      printf(") => ");
      for (unsigned long i = 0; i < _get_header(ast->formal_type.type_v_v)->size; i++) {
        for (unsigned long j = 0; j < _get_header(ast->formal_type.type_v_v[i])->size; j++) {
          print_AST(ast->formal_type.type_v_v[i] + j);
          printf(" ");
        }
        printf(", ");
      }
      printf("\b");
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
static TrieNode* type_trie;
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
  (*tokens_ptr)++;
  longjmp(jumping_buf, 0);
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

// Handles failures
static inline ASTNode* parse_group(
  Token** tokens_ptr, bool (*is_delim)(TokenType), bool (*is_allowed_term)(TokenType), 
  ASTNode* expr,      TermType what_is_type
) {

  Token* tokens = *tokens_ptr;
  for (; !(*is_delim)(tokens->type); tokens++) {
    if (!(*is_allowed_term)(tokens->type))
      handle(&tokens, "Unexpected token");

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
    push(data_decl, token_to_term(TYPE, (**tokens_ptr)));
    (*tokens_ptr)++;
    return data_decl;
  }
  if ((*tokens_ptr)->type != TYPE_K)
    handle(tokens_ptr, "Expected type");
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
      handle(tokens_ptr, "Must bind to a name");
    if (!verify_types(tokens_ptr, equals, elemsof(equals))) {
      parse_value_expression(tokens_ptr, vdef->variable_definition.arguments_v);
      if(verify_types(tokens_ptr, equals, elemsof(equals)))
        handle(tokens_ptr, "Expected equals");
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
    group_vec = (*parse_func)(tokens_ptr, group_vec);
    push(aggregate_vec, group_vec);
  } while (in_delims((*tokens_ptr)->type, delims, delims_s));
  return aggregate_vec;
}

static inline ASTNode** parse_constraint(Token** tokens_ptr) {
  // TODO: This will skip a constraint if its empty (`example :: (Constraint a, C) => ...` wont throw an error and catch `C`), fix later but idc for now
  ASTNode** constraints = new_vector_with_capacity(*constraints, 4); // NOLINT(bugprone-sizeof-expression)
  
add_constraint:
  if (!verify_types(tokens_ptr, constraint_types, elemsof(constraint_types)))
    handle(tokens_ptr, "Unexpected token");

  // TODO: Replace this for a binary search ?
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
    new_var[0] = token_to_term(TYPE, (*tokens_ptr)[-1]);
    new_var[1] = (ASTNode) { .expression_v = new_constraints };

    push(constraints, new_var);
  } 

  if (verify_types(tokens_ptr, comas, elemsof(comas)))
    goto add_constraint;

  return constraints;
}

static inline Type** parse_type(Token** tokens_ptr, ASTNode** constraints) {
  Type** type_v = new_vector_with_capacity(*type_v, 4); // NOLINT(bugprone-sizeof-expression)
  signed short ctx = 0x8000;
  bool not_final_arrow = true;

add_type:
  if ((*tokens_ptr)->type == IDENTIFIER) {
    Type* type = new_vector_with_capacity(*type, 1);
    unsigned long ip = 0, constraints_ptr = 0;
    if (constraints) {
      for_each(i, constraints) {
        if (strcmp(constraints[i]->term.name, (*tokens_ptr)->token)) {
          push(type, (Type) { .var = ((TypeVar) { .info = 0x8000 | (signed short) i, .ast_ptr = (unsigned long) (constraints[i] + i) & 0x0000ffffffffffff })});
          goto continue_push;
        }
      } 
    }
    push(type, (Type) { .var = ((TypeVar) { .info = ctx++, .ast_ptr = NULL } ) });
  continue_push:
    push(type_v, type);
    
    goto check_coma;
  }
  if (!verify_types(tokens_ptr, type_k, elemsof(type_k)))
    handle(tokens_ptr, "Type must only either contain a sole type variable or a higher order type constructor");

  Type* type = new_vector_with_capacity(*type, 4);
  TypeConcrete c = follow_pattern_with_default((*tokens_ptr)[-1].token, type_trie, 0);
  if (!c) handle(tokens_ptr, "Type did not exist");

  for (; (*tokens_ptr)->type == IDENTIFIER; (*tokens_ptr)++) {
    if (constraints) for_each(i, constraints) {
      if (strcmp(constraints[i]->term.name, (*tokens_ptr)->token)) {
        push(type, ((Type) { .var = (TypeVar) { .info =  0x8000 | (signed short) i, .ast_ptr = (unsigned long) (constraints[i] + 1) & 0x0000ffffffffffff } }));
        continue;
      }
    }
    push(type, (Type) { .var = ((TypeVar) { .info = ctx++, .ast_ptr = NULL } ) });
  }

  push(type, (Type) { .conc = c })
check_coma:
  if (not_final_arrow && verify_types(tokens_ptr, comas, elemsof(comas))) goto add_type;
  if (not_final_arrow && verify_types(tokens_ptr, arrow, elemsof(arrow))) {
    not_final_arrow = false;
    goto add_type;
  }
}

AST parser(Token* tokens, Token** infixes, Error* _error_buf) {
  _Static_assert(1 << sizeof(ExpStackIndex) <= MAX_PARENTHESIS  , "ExpStackIndex must be able to entirely index parenthesis stack");
  _Static_assert(sizeof(PrecInfo) <= sizeof(unsigned long)      , "Cannot fit PrecInfo into unsigned long");
  _Static_assert(sizeof(void*) == sizeof(unsigned long)         , "Pointers must be of 64-bits");

  TrieNode* ASTrie = create_node(0, -1);
  type_trie = create_node(0, -1);
  TrieNode* instance_trie = create_node(0, -1);
  precedence_trie = create_node(0, -1);
  error_buf = _error_buf;

  Token* infix = infixes[0];
  for_each_element(infix, infixes) {
    // blah blah blah errors
    insert_trie(infix[2].token, mkPrecInfo(atoi(infix[1].token), infix[0].type == INFIXR), precedence_trie);
  }

  for (Token token = tokens[0]; token.type != EndOfFile; token = tokens[0]) {
    switch (token.type) {
      case INFIXL:
      case INFIXR:
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
        skip_to_tok = SEMI_COLON;
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
            formal_type->formal_type.constraints_v_v = parse_constraint(&tokens);
            if (!verify_types(&tokens, close_constaint, elemsof(close_constaint))) handle(&tokens, "Expecting closing constraint sequence");
          }
          formal_type->formal_type.type_v_v = parse_type(&tokens, formal_type->formal_type.constraints_v_v);
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
        func_impl->implementation.body_v = parse_sep_by(&tokens, parse_expression, comas, elemsof(comas), 4);
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
        skip_to_tok = SEMI_COLON;
        tokens++;
        ASTNode** constraints = NULL;
        if (tokens->type == OPEN_PAREN) {
          tokens++;
          constraints = parse_constraint(&tokens);
        }
        char* name = tokens->token;
        ASTNode* constructs = new_vector_with_capacity(*constructs, 4);
        ASTNode** constructors = parse_sep_by(&tokens, parse_type_expression, bars, elemsof(bars), 4);
        ASTNode* def;
        ASTNode* constructor = constructors[0];
        for_each_element(constructor, constructors) {
          push(constructor, *constructs);
          ASTNode* formal_type = Malloc(sizeof(*formal_type));
          *formal_type = (ASTNode) {
          };
          ASTNode* constructor_def = Malloc(sizeof(*constructor_def));
          *constructor_def = (ASTNode) {
            .type = F_DEFINITION,
            .function_definition.declaration = formal_type,
            .function_definition.implementations_v = NULL
          };
          insert_trie(constructor[0].term.name, (unsigned long) constructor_def, ASTrie);
        }
        insert_trie(name, (unsigned long) constraints, type_trie);
        break;
      }
      case INSTANCE:
      case CLASS:
        skip_to_tok = CLOSE_CURLY;
        handle(&tokens, "Not supported, sorry");
        break;
      default:
        printf("%d\n", token.type);
        tokens++;
        break;
    }
    setjmp(jumping_buf);
  }

  return (AST) {
    .is_correct_ast = is_correct_ast,
    .astrie = ASTrie,
    .type_trie = type_trie,
    .instance_trie = instance_trie,
    .error_buf = error_buf
  };
}

#ifdef DEBUG
#pragma GCC pop_options
#endif

/** TODO:
 * - If then else, vector/tuple literals
 * - Upgrade handle to take a sequence of tokens sorted by preference on when to stop
 * - Instances/Classes
 */
