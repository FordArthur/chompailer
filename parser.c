#include "parser.h"

#define token_to_term(termtype, tok) (ASTNode) {.type = TERM, .term.type = termtype, .term.index = tok.index, .term.line = tok.line, .term.length = tok.length, .term.name = tok.token}

typedef struct PrecInfo {
  unsigned long precedence;
  bool is_infixr;
} PrecInfo;

typedef struct PrecEntry {
  PrecInfo info;
  ASTNode* node;
} PrecEntry;


inline PrecInfo get_precedence(const char* name) {
  // ...
}

void print_ast(ASTNode* ast) {
  switch (ast->type) {
    case TERM:
      printf("%s", ast->term.name);
      break;
    case EXPRESSION:
      for_each(i, ast->expression) {
	print_ast(ast + i);
	printf(" ");
      }
      printf("\n");
      break;
    case BIN_EXPRESSION:
      print_ast(ast->bin_expression.operator);
      printf("\t"); print_ast(ast->bin_expression.left_expression); printf("\n");
      printf("\t"); print_ast(ast->bin_expression.right_expression); printf("\n"); 
      break;
    case DECLARATION:
      break;
    case F_DEFINITION:
      break;
    case V_DEFINITION:
      break;
  }
}

AST parser(Token* tokens, Token** infixes, Error* error_buf) {
  ASTNode* ast = new_vector_with_capacity(*ast, 16);
  bool is_correct_ast = true;

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

  // Build table for infixes
  // ...

  // Iterate over tokens
  for (; tokens->type != _EOF; tokens++) {
    switch (tokens->type) {
      case OPEN_PAREN:
	expr_stack_top++
        expr_stack[expr_stack_top] = curexpr;
        curexpr = new_vector_with_capacity(*curexpr, 8);
        break;
      case CLOSE_PAREN:
	// if expr_stack_top == 0 then err
	expr_stack_top--;
        push(expr_stack[expr_stack_top], *curexpr); // actually, push root node of the popped expr
        curexpr = expr_stack[expr_stack_top];
        break;
      case NATURAL:
        push(curexpr, token_to_term(TNATURAL, (*tokens)));
        break;
      case REAL:
        push(curexpr, token_to_term(TREAL, (*tokens)));
        break;
      case CHARACTER:
        push(curexpr, token_to_term(TCHARACTER, (*tokens)));
        break;
      case STRING:
        push(curexpr, token_to_term(TSTRING, (*tokens)));
        break;
      case IDENTIFIER:
        // Could be a variable as well!
        // (i.e. could be a function just that has type `a`)
        push(curexpr, token_to_term(FUNCTION, (*tokens)));
        break;
      case OPERATOR:
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


        break;
      case SEMI_COLON:
        if (expr_stack_top) {
          is_correct_ast = false;
          push(error_buf, mkerr(PARSER, tokens->line, tokens->index, "Unmatched parenthesis before semicolon"));
          expr_stack_top = 0;
        }
	push(ast, *curexpr); // push root node
        curexpr = new_vector_with_capacity(*curexpr, 8);
      case COMMENT:
        break;
    }
  }
}
