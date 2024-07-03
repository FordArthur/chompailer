#include "parser.h"
#include "scanner.h"
#include "vec.h"

#define token_to_term(termtype, tok) (ASTNode) {.type = TERM, .term.type = termtype, .term.index = tok.index, .term.line = tok.line, .term.length = tok.length, .term.name = tok.token}

typedef struct ExpressionStack {
  unsigned long argument_count;
  Token* expression;
} ExpressionStack;

typedef struct ;

inline unsigned long get_arg_length(const char* name) {
  // ...
  return 0;
}



AST parser(Token* tokens, Priority* infixes) {
  Token* ast = new_vector_with_capacity(*ast, 16);
  Token* leftexp = new_vector_with_capacity(*leftexp, 8);
  Token* rightexp = new_vector_with_capacity(*rightexp, 8);

  Token* precedence_table[MAX_PRECEDENCE] = {0}; // It can store up to MAX_PRECEDENCE/2 tokens
					   	 // since we only track of the lowest and highest
					   	 // tokens in any precedence succession

  ExpressionStack exp_stack[MAX_PARENTHESIS] = {0}; // Here we utilize the entire size 
  // !! Type depends on MAX_PARENTHESIS !!
  unsigned char exp_stack_top = 0;
  
  unsigned long cur_arg_count = 0;
  Token* curexp = new_vector_with_capacity(*curexp, 8);

  bool waiting_rightexpr = false;

  // Build table for infixes
  // ...

  // Iterate over tokens
  for (; tokens->type != _EOF; tokens++) {
    switch (tokens->type) {
      case OPEN_PAREN:
	exp_stack[curexp_stack_top++] = (ExpressionStack) {
	  .argument_count = cur_arg_count,
	  .expression = curexp
	};
	curexp = new_vector_with_capacity(*curexp, 8);
	break;
      case CLOSE_PAREN:
	push(exp_stack[--exp_stack_top], curexp);
	curexp = exp_stack[exp_stack_top];
	cur_arg_count = exp_stack[exp_stack_top];
	break;
      case IDENTIFIER:
	if (!cur_arg_count) {
	  cur_arg_count = get_arg_length(tokens->token);
	  push(curexp, token_to_term(cur_arg_count? FUNCTION : VARIABLE, (*tokens)));
	} else {
	  cur_arg_count--;
	  push(curexp, token_to_term(VARIABLE, (*tokens)));
	}
        break;
      case OPERATOR:
	waiting_rightexpr = true;
	leftexpr = curexp;
	curexp = new_vector_with_capacity(*curexp, 1);
	push(curexp, token_to_term(FUNCTION, (*tokens)));
	break;
      case LINE:
	// ...
	// if (whatever) skip;
      case SEMICOLON:
	push(ast, curexp);
	curexp = new_vector_with_capacity(*curexp, 8);
      case COMMENT:
	break;
    }
  }
}
