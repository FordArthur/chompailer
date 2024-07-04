#include "parser.h"
#include "scanner.h"
#include "vec.h"

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

AST parser(Token* tokens, Token** infixes, Error* error_buf) {
  ASTNode* ast = new_vector_with_capacity(*ast, 16);
  bool is_correct_ast = true;

  ASTNode* curexp = new_vector_with_capacity(*curexp, 8);
  bool waiting_rightexpr = false;

  PrecEntry precedence_table[MAX_PRECEDENCE] = {0}; // It can store up to MAX_PRECEDENCE/2 tokens
  // since we only track of the lowest and highest
  // tokens in any precedence succession

  ASTNode* exp_stack[MAX_PARENTHESIS] = {0}; // Here we utilize the entire size 

  // !! Type depends on MAX_PARENTHESIS !!
  unsigned char exp_stack_top = 0;

  PrecInfo curprecedence;
  PrecEntry belowprecnode;

  // Build table for infixes
  // ...

  // Iterate over tokens
  for (; tokens->type != _EOF; tokens++) {
    switch (tokens->type) {
      case OPEN_PAREN:
        exp_stack[exp_stack_top++] = curexp;
        curexp = new_vector_with_capacity(*curexp, 8);
        break;
      case CLOSE_PAREN:
        push(exp_stack[--exp_stack_top], *curexp);
        curexp = exp_stack[exp_stack_top];
        break;
      case NATURAL:
        push(curexp, token_to_term(TNATURAL, (*tokens)));
        if (waiting_rightexpr) {
          curexp = new_vector_with_capacity(*curexp, 8);
          waiting_rightexpr = false;
        }
        break;
      case REAL:
        push(curexp, token_to_term(TREAL, (*tokens)));
        if (waiting_rightexpr) {
          curexp = new_vector_with_capacity(*curexp, 8);
          waiting_rightexpr = false;
        }
        break;
      case CHARACTER:
        push(curexp, token_to_term(TCHARACTER, (*tokens)));
        if (waiting_rightexpr) {
          curexp = new_vector_with_capacity(*curexp, 8);
          waiting_rightexpr = false;
        }
        break;
      case STRING:
        push(curexp, token_to_term(TSTRING, (*tokens)));
        if (waiting_rightexpr) {
          curexp = new_vector_with_capacity(*curexp, 8);
          waiting_rightexpr = false;
        }
        break;
      case IDENTIFIER:
        // Could be a variable as well!
        // (i.e. could be a function just that has type `a`)
        push(curexp, token_to_term(FUNCTION, (*tokens)));
        if (waiting_rightexpr) {
          curexp = new_vector_with_capacity(*curexp, 8);
          waiting_rightexpr = false;
        }
        break;
      case OPERATOR:
        // check if waiting, and if so err
        curprecedence = get_precedence(tokens->token);
        for (unsigned long i = curprecedence.precedence; i >= 0; i--)
          if ((belowprecnode = precedence_table[i]).node)
            break;
        // careful when the if doesnt run

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
          new_bin_expr->bin_expression.right_expression = curexp;
          waiting_rightexpr = true;
          // insert elements in table
        } else { 
          ASTNode* rightexpr;
          rightexpr = belowprecnode.node->bin_expression.right_expression;
          ASTNode* new_bin_expr = new_vector_with_capacity(*new_bin_expr, 1);
          new_bin_expr->type = BIN_EXPRESSION;
          new_bin_expr->bin_expression.left_expression = rightexpr;
          new_bin_expr->bin_expression.right_expression = curexp;
          waiting_rightexpr = true;
          // insert elements in table
        }


        break;
      case SEMI_COLON:
        if (exp_stack_top) {
          is_correct_ast = false;
          push(error_buf, mkerr(PARSER, tokens->line, tokens->index, "Unmatched parenthesis before semicolon"));
          exp_stack_top = 0;
        } else
        push(ast, *curexp);
        curexp = new_vector_with_capacity(*curexp, 8);
      case COMMENT:
        break;
    }
  }
}
