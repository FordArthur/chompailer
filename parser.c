#include "parser.h"

#define token_to_term(termtype, tok) (ASTNode) {.type = TERM, .term.type = termtype, .term.index = tok.index, .term.line = tok.line, .term.length = tok.length, .term.name = tok.token}

 // !! It's really important that the size of this struct isnt greater than unsigned long !!
typedef struct PrecInfo {
  unsigned int precedence;
  bool is_infixr;
} PrecInfo;

typedef struct PrecEntry {
  PrecInfo info;
  ASTNode* node;
} PrecEntry;

static TrieNode* precedence_trie;

static inline unsigned long mkPrecInfo(unsigned int precedence, bool is_infixr) {
  PrecInfo precinfo = (PrecInfo) {.precedence = precedence, .is_infixr = is_infixr};
  return *(unsigned long*) &precinfo;
}

static inline PrecInfo get_precedence(char* name) {
  // TODO: Emit warning when not in trie
#ifdef DEBUG
  printf("Searching for %s in precedence trie:\n", name);
  print_trie(precedence_trie);
  fflush(stdout);
#endif
  PrecInfo _default = (PrecInfo) { .precedence = 6, .is_infixr = false };
  unsigned long res = follow_pattern_with_default(name, precedence_trie, *(unsigned long*) &_default);
  return *(PrecInfo*) &res;
}

void print_AST(ASTNode* ast) {
  switch (ast->type) {
    case TERM:
      printf("%s", ast->term.name);
      break;
    case EXPRESSION:
      for_each(i, ast->expression) {
        print_AST(ast->expression + i);
        printf(" ");
      }
      break;
    case BIN_EXPRESSION:
      print_AST(ast->bin_expression.operator);
      printf("\t"); print_AST(ast->bin_expression.left_expression); printf("\n");
      printf("\t"); print_AST(ast->bin_expression.right_expression); printf("\n"); 
      break;
    case DECLARATION:
      break;
    case F_DEFINITION:
      break;
    case V_DEFINITION:
      break;
  }
  printf("\n");
}

AST parser(Token* tokens, Token** infixes, Error* error_buf) {
  _Static_assert(sizeof(PrecInfo) <= 8, L"Cannot fit PrecInfo into unsigned long");
  ASTNode* ast = new_vector_with_capacity(*ast, 16);
  bool is_correct_ast = true;

  // have something where instead of pushing this it pushes that thing which points to curexpr
  // or something
  ASTNode* curexpr = new_vector_with_capacity(*curexpr, 8);
  bool waiting_rightexpr = false;

  bool uninitialised_index_table = true;
  // !! Type depends on MAX_PRECEDENCE !!
  unsigned char precedence_index_table[MAX_PRECEDENCE];
  PrecEntry precedence_table[MAX_PRECEDENCE] = {0}; // It can store up to MAX_PRECEDENCE/2 tokens
						    // since we only track of the lowest and highest
						    // tokens in any precedence succession

  ASTNode* expr_stack[MAX_PARENTHESIS] = {0}; // Here we utilize the entire size 

  // !! Type depends on MAX_PARENTHESIS !!
  unsigned char expr_stack_top = 0;

  PrecInfo curprecedence;
  PrecEntry belowprecnode;
  
  precedence_trie = create_node(0, -1);
  insert_trie("+", mkPrecInfo(5, false), precedence_trie);
  insert_trie("*", mkPrecInfo(5, false), precedence_trie);
  // ...
  
  // Iterate over tokens
  for (; tokens->type != _EOF; tokens++) {
#ifdef DEBUG
  bool printable = true;
#endif
    switch (tokens->type) {
      case OPEN_PAREN:
#ifdef DEBUG
	printf("OPEN_PAREN\n");
#endif
        expr_stack_top++;
        expr_stack[expr_stack_top] = curexpr;
        curexpr = new_vector_with_capacity(*curexpr, 8);
        break;
      case CLOSE_PAREN:
#ifdef DEBUG
	printf("CLOSE_PAREN\n");
#endif
        // if expr_stack_top == 0 then err
        expr_stack_top--;
        push(expr_stack[expr_stack_top], *curexpr); // actually, push root node of the popped expr
        curexpr = expr_stack[expr_stack_top];
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
      case IDENTIFIER:
#ifdef DEBUG
	printf("IDENTIFIER (%s)\n", tokens->token);
#endif
        // Could be a variable as well!
        // (i.e. could be a function just that has type `a`)
        push(curexpr, token_to_term(FUNCTION, (*tokens)));
        break;
      case OPERATOR:
#ifdef DEBUG
	printf("OPERATOR (%s)\n", tokens->token);
#endif
        curprecedence = get_precedence(tokens->token);
        if (uninitialised_index_table) {
          uninitialised_index_table = false;
          // !! Type depends on MAX_PRECEDENCE !!
          for (unsigned char i = 0; i < MAX_PRECEDENCE; i++)
            precedence_index_table[i] = curprecedence.precedence;

          ASTNode* new_bin_expr = new_vector_with_capacity(*new_bin_expr, 1);
          new_bin_expr->type = BIN_EXPRESSION;
          new_bin_expr->bin_expression.left_expression = curexpr;
          curexpr = new_vector_with_capacity(*curexpr, 8);
          new_bin_expr->bin_expression.right_expression = curexpr;
          waiting_rightexpr = true;

          precedence_table[curprecedence.precedence] = (PrecEntry) {
            .info = curprecedence,
            .node = new_bin_expr
          };
          break;
        }
#ifdef DEBUG
	printf("Precedence_index_table: [");
	for (unsigned char i = 0; i < MAX_PRECEDENCE; i++) {
	  printf("%d, ", precedence_index_table[i]);
	}
	printf("]\n");
	fflush(stdout);
#endif

	printf("\\OPERATOR\n");
        belowprecnode = precedence_table[precedence_index_table[curprecedence.precedence]];


        if (curprecedence.precedence == belowprecnode.info.precedence && curprecedence.is_infixr != belowprecnode.info.is_infixr) {
          is_correct_ast = false;
          push(error_buf, mkerr(PARSER, tokens->line, tokens->index, "Cannot mix operators that associate left and right"));
          while (tokens->type != SEMI_COLON) tokens++;
          break;
        }

        if (curprecedence.precedence > belowprecnode.info.precedence + curprecedence.is_infixr*(curprecedence.precedence == belowprecnode.info.precedence)) {
          ASTNode* new_bin_expr = new_vector_with_capacity(*new_bin_expr, 1);
          new_bin_expr->type = BIN_EXPRESSION;
          new_bin_expr->bin_expression.left_expression = belowprecnode.node;
          new_bin_expr->bin_expression.right_expression = curexpr;
          waiting_rightexpr = true;
        } else { 
          ASTNode* rightexpr;
          rightexpr = belowprecnode.node->bin_expression.right_expression;
          ASTNode* new_bin_expr = new_vector_with_capacity(*new_bin_expr, 1);
          new_bin_expr->type = BIN_EXPRESSION;
          new_bin_expr->bin_expression.left_expression = rightexpr;
          new_bin_expr->bin_expression.right_expression = curexpr;
          waiting_rightexpr = true;
        }

	printf("\\OPERATOR\n");

        break;
      case SEMI_COLON:
        if (expr_stack_top) {
          is_correct_ast = false;
          push(error_buf, mkerr(PARSER, tokens->line, tokens->index, "Unmatched parenthesis before semicolon"));
          expr_stack_top = 0;
        }
#ifdef DEBUG
	print_AST(curexpr);
	printf("- - -\n");
#endif
        push(ast, *(uninitialised_index_table? curexpr : precedence_table[precedence_index_table[0]].node));
        curexpr = new_vector_with_capacity(*curexpr, 8);
#ifdef DEBUG
	printable = false;
#endif
	break;
      case COMMENT:
        break;
    }
#ifdef DEBUG
    if (printable) {
	print_AST(ast);
	printf("---\n");
    }
#endif
  }
  return (AST) {
	.is_correct_ast = is_correct_ast,
	.ast = ast,
	.error_buf = error_buf
  };
}
