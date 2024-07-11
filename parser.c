#include "parser.h"

#define token_to_term(termtype, tok) (ASTNode) {.type = TERM, .term.type = termtype, .term.index = tok.index, .term.line = tok.line, .term.length = tok.length, .term.name = tok.token}

// !! It's really important that the size of this struct isnt greater than unsigned long !!
typedef struct PrecInfo {
  unsigned int prec;
  bool is_infixr;
} PrecInfo;

typedef struct ExpStackEntry {
  ASTNode* root;
  ASTNode* prev_expr;
} ExpStackEntry;

static TrieNode* precedence_trie;

static inline unsigned long mkPrecInfo(unsigned int precedence, bool is_infixr) {
  PrecInfo precinfo = (PrecInfo) {.prec = precedence, .is_infixr = is_infixr};
  return *(unsigned long*) &precinfo;
}

static inline void restart_expression(ASTNode** expression, ASTNode** expression_ptr) {
  *expression = new_vector_with_capacity(**expression, 8);
  *expression_ptr = Malloc(sizeof(**expression_ptr));
  (*expression_ptr)->type = EXPRESSION;
  (*expression_ptr)->expression = *expression;
}

static inline PrecInfo get_precedence(char* name) {
  // TODO: Emit warning when not in trie
#ifdef DEBUG
  printf("Searching for %s in precedence trie:\n", name);
  print_trie(precedence_trie);
#endif
  PrecInfo _default = (PrecInfo) { .prec = 6, .is_infixr = false };
  unsigned long res = follow_pattern_with_default(name, precedence_trie, *(unsigned long*) &_default);
  return *(PrecInfo*) &res;
}

void _print_AST(volatile ASTNode* ast, unsigned long tab) {
  if (!ast) {
    printf(":: To be inferred");
    return;
  }
  for (unsigned long pad = tab; pad; pad--) printf("\t"); 
  switch (ast->type) {
    case TERM:
      printf("%s", ast->term.name);
      break;
    case EXPRESSION:
      for_each(i, ast->expression) {
        _print_AST(ast->expression + i, 0);
        printf(" ");
      }
      break;
    case BIN_EXPRESSION:
      printf("%s\n", ast->bin_expression.op->term.name);
      _print_AST(ast->bin_expression.left_expression, tab + 1); printf("\n");
      _print_AST(ast->bin_expression.right_expression, tab + 1); printf("\n"); 
      break;
    case DECLARATION:
      _print_AST(ast->declaration.expression, tab);
      printf(":: ");
      _print_AST(ast->declaration.type, 0);
      break;
    case IMPLEMENTATION:
      for_each(i, ast->implementation.arguments) {
        _print_AST(ast->implementation.arguments + i, 0);
        printf(" ");
      }
      printf("= ");
      for_each(i, ast->implementation.body) {
        _print_AST(ast->implementation.body + i, 0);
        printf("\n");
      }
      break;
    case V_DEFINITION:
      printf("vdef\n");
      break;
    case F_DEFINITION:
      _print_AST(ast->function_definition.declaration, tab);
      printf("\n");
      for_each(i, ast->function_definition.implementations)
      _print_AST(ast->function_definition.implementations + i, tab);
      break;
  }
}

AST parser(Token* tokens, Token** infixes, Error* error_buf) {
  _Static_assert(sizeof(PrecInfo) <= sizeof(unsigned long), "Cannot fit PrecInfo into unsigned long");

  TrieNode* ASTrie = create_node(0, -1);

  ASTNode* expressions = NULL;
  bool is_correct_ast = true;

  ASTNode* curexpr;
  ASTNode* curexpr_ptr;
  restart_expression(&curexpr, &curexpr_ptr);
  ASTNode* root = curexpr_ptr;
  ASTNode* impl = NULL;

  ExpStackEntry expr_stack[MAX_PARENTHESIS] = {0}; // Here we utilize the entire size 

  // !! Type depends on MAX_PARENTHESIS !!
  unsigned char expr_stack_top = 0;

  PrecInfo curprec;

  precedence_trie = create_node(0, -1);
  insert_trie("+", mkPrecInfo(6, false), precedence_trie);
  insert_trie("*", mkPrecInfo(5, false), precedence_trie);
  // ...

  bool is_parsing_type = false;
  bool expecting_type = false;

  bool expecting_assign = false;
  bool expecting_equals = true;

  // Iterate over tokens
  for (; tokens->type != _EOF; tokens++) {
    if (expecting_type && (tokens->type != TYPE_K && tokens->type != IDENTIFIER)) {
#ifdef DEBUG
      printf("Got: %d\n", tokens->type);

#endif /* ifdef DEBUG */
      is_correct_ast = false;
      push(
        error_buf, 
        mkerr(PARSER, tokens->line, tokens->index, "Must provide types or generics for declarations"));
      continue;
    }
    switch (tokens->type) {
      case OPEN_PAREN:
#ifdef DEBUG
      printf("OPEN_PAREN\n");
#endif
      expr_stack[expr_stack_top] = (ExpStackEntry) {
        .root = root,
        .prev_expr = curexpr
      };
      expr_stack_top++;
      restart_expression(&curexpr, &curexpr_ptr);
      root = curexpr_ptr;
      break;
      case CLOSE_PAREN:
#ifdef DEBUG
      printf("CLOSE_PAREN\n");
#endif
      if (!expr_stack_top) {
        is_correct_ast = false;
        push(
          error_buf,
          mkerr(PARSER, tokens->line, tokens->index, "Unmatched parenthesis");
        )
        break;
      }
      expr_stack_top--;
      push(expr_stack[expr_stack_top].prev_expr, *root);
      root = expr_stack[expr_stack_top].root;
      break;
      case ARROW:
        if (!is_parsing_type) {
          push(
            error_buf, 
            mkerr(PARSER, tokens->line, tokens->index, "Extraneous arrow")
          );
        }
        is_parsing_type = false;
        expecting_type = true;
        break;
      case COMA:
        if (is_parsing_type)
          expecting_type = true;
        break;
      case LET:
        // some error idk
        expecting_assign = true;
        break;
      case NATURAL:
#ifdef DEBUG
      printf("NATURAL\n");
#endif
      push(curexpr, token_to_term(TNATURAL, (*tokens)));
      break;
      case REAL:
#ifdef DEBUG
      printf("REAL\n");
#endif
      push(curexpr, token_to_term(TREAL, (*tokens)));
      break;
      case CHARACTER:
#ifdef DEBUG
      printf("CHARACTER\n");
#endif
      push(curexpr, token_to_term(TCHARACTER, (*tokens)));
      break;
      case STRING:
#ifdef DEBUG
      printf("STRING\n");
#endif
      push(curexpr, token_to_term(TSTRING, (*tokens)));
      break;
      case TYPE_K:
      TYPE:
        push(curexpr, token_to_term(TYPE, (*tokens)));
        expecting_type = false;
        break;
      case IDENTIFIER:
#ifdef DEBUG
      printf("IDENTIFIER (%s)\n", tokens->token);
#endif
      // Could be a variable as well!
      // (i.e. could be a function just that has type `a`)
      if (expecting_type)
        goto TYPE;
      push(curexpr, token_to_term(FUNCTION, (*tokens)));
      break;
      case OPERATOR: {
#ifdef DEBUG
        printf("op (%s)\n", tokens->token);
#endif
        PrecInfo curprec = get_precedence(tokens->token);

        ASTNode* new_bin_op = Malloc(sizeof(*new_bin_op));
        *new_bin_op = token_to_term(FUNCTION, (*tokens));

        ASTNode* new_bin_expr = Malloc(sizeof(*new_bin_expr));
        new_bin_expr->type = BIN_EXPRESSION;
        new_bin_expr->bin_expression.op = new_bin_op;

        ASTNode* above_node = root;
        ASTNode* prev_above_node;

        bool in_root = true;
        while (
          above_node->type == BIN_EXPRESSION 
          && get_precedence(above_node->bin_expression.op->term.name).prec 
          + curprec.is_infixr * (curprec.prec == get_precedence(above_node->bin_expression.op->term.name).prec) 
          > curprec.prec
        ) {
#ifdef DEBUG
          printf("Traversing...\n", tokens->token);
          print_AST(above_node);
#endif
          prev_above_node = above_node;
          above_node = above_node->bin_expression.right_expression;
          in_root = false;
        }

        new_bin_expr->bin_expression.left_expression = above_node;
        restart_expression(&curexpr, &curexpr_ptr);
        new_bin_expr->bin_expression.right_expression = curexpr_ptr;

        if (in_root)
          root = new_bin_expr;
        else 
          prev_above_node->bin_expression.right_expression = new_bin_expr;

        break;
      }
      case SEMI_COLON:
        if (expr_stack_top) {
          is_correct_ast = false;
          push(error_buf, mkerr(PARSER, tokens->line, tokens->index, "Unmatched parenthesis before semicolon"));
          expr_stack_top = 0;
        }
        if (expressions)
          push(expressions, *root);
        restart_expression(&curexpr, &curexpr_ptr);
        root = curexpr_ptr;
        expecting_equals = true;
        is_parsing_type = false;
        break;
      case EQUALS:
        if (!expecting_assign && !expecting_equals) {
          is_correct_ast = false;
          push(
            error_buf,
            mkerr(PARSER, tokens->line, tokens->index, "Cannot start another function definition without a semicolon before")
          );
          break;
        }
        if (is_empty(curexpr) || curexpr[0].type != TERM) {
          is_correct_ast = false;
          push(
            error_buf,
            mkerr(PARSER, tokens->line, tokens->index, is_empty(curexpr)? "Provide a function name" : "Function name must be identifier"));
          break;
        }
        if (expecting_assign) {
          ASTNode* vdef = Malloc(sizeof(*vdef));
          vdef->type = V_DEFINITION;
          vdef->implementation.arguments = new_vector_with_capacity(*vdef->implementation.arguments, 4);
          for (unsigned long arg_index = 1; arg_index < _get_header(curexpr)->size; arg_index++)
            push(vdef->implementation.arguments, curexpr[arg_index]);
          restart_expression(&curexpr, &curexpr_ptr);
          vdef->variable_definition.expression = curexpr_ptr;
          break;
        }
        char* name = curexpr[0].term.name;
        impl = Malloc(sizeof(*impl));
        impl->type = IMPLEMENTATION;
        impl->implementation.arguments = new_vector_with_capacity(*impl->implementation.arguments, 4);
        for (unsigned long arg_index = 1; arg_index < _get_header(curexpr)->size; arg_index++)
          push(impl->implementation.arguments, curexpr[arg_index]);
        restart_expression(&curexpr, &curexpr_ptr);
        root = curexpr_ptr;
        impl->implementation.body = expressions = new_vector_with_capacity(*expressions, 16);
        ASTNode* def;
        if ((def = (ASTNode*)follow_pattern_with_default(name, ASTrie, 0))) {
          push(def->function_definition.implementations, *impl);
        }
        else {
          def = Malloc(sizeof(*def));
          def->type = F_DEFINITION;
          def->function_definition.implementations = new_vector_with_capacity(*def->function_definition.implementations, 4);
          push(def->function_definition.implementations, *impl);
          def->function_definition.declaration = NULL;
          insert_trie(name, (unsigned long) def, ASTrie);
          expecting_equals = false;
        }
        break;
      case DOUBLE_COLON: {
        char* name = curexpr[0].term.name;
        ASTNode* decl = Malloc(sizeof(*decl));
        decl->type = DECLARATION;
        decl->declaration.expression = curexpr_ptr;
        restart_expression(&curexpr, &curexpr_ptr);
        decl->declaration.type = curexpr_ptr;
        root = decl;
        is_parsing_type = true;
        expecting_type = true;

        if ((def = (ASTNode*)follow_pattern_with_default(name, ASTrie, 0))) {
          def->function_definition.declaration = decl;
        } else {
          def = Malloc(sizeof(*def));
          def->type = F_DEFINITION;
          def->function_definition.declaration = decl;
          def->function_definition.implementations = new_vector_with_capacity(*def->function_definition.implementations, 4);
          insert_trie(name, (unsigned long) def, ASTrie);
        }
        expressions = new_vector_with_capacity(*expressions, 16);
        break;
      }
      case COMMENT:
        break;
    }
  }
  return (AST) {
    .is_correct_ast = is_correct_ast,
    .astrie = ASTrie,
    .error_buf = error_buf
  };
}

/* 
 * To fix:
 * without an arrow, it just keeps thinking everything is a type
 * add lets to astrie
 */
