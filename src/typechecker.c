#include "typechecker.h"
#include "compiler_inner_types.h"
#include "parser.h"
#include "trie.h"
#include "vec.h"
#include <setjmp.h>
#include <string.h>

typedef ASTNode* Instances;

typedef enum TypeKind {
  CONSTRUCTOR, VAR, CONCRETE, FUNC,
  ANY = -1,
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
    struct {
      struct Type* func_type;
      bool from_class;
    } func;
  };
} Type;

static bool passes_type_check = true;
static Error* error_buf;
static TrieNode* funcs_trie;
static TrieNode* type_trie;
static TrieNode* instance_trie;
static TrieNode* class_trie;
static jmp_buf jumping_buf;

static inline void handle(ASTNode* blame, char* err_msg) {
  passes_type_check = false;
  push(error_buf, mkerr(TYPECHECKER, blame->term.line, blame->term.index, err_msg));
  longjmp(jumping_buf, 0);
}

// Since the pointer is only for comparing (as it's unique), putting values like these will also work
static Type 
  _Chompa_Builtin_Natural   = (Type) { .kind = CONCRETE, .concrete = (ASTNode*) 0 }
 ,_Chompa_Builtin_Character = (Type) { .kind = CONCRETE, .concrete = (ASTNode*) 1 }
 ,_Chompa_Builtin_Real      = (Type) { .kind = CONCRETE, .concrete = (ASTNode*) 2 }
 ,_Chompa_Builtin_String    = (Type) { .kind = CONCRETE, .concrete = (ASTNode*) 3 };

typedef struct ScopeEntry {
  char* name;
  Type type;
} ScopeEntry;

typedef struct InstanceEntry {
  char* name;
  ASTNode* implementations;
} InstanceEntry;

static ScopeEntry* scope_stack;

static inline Type search_scope(ASTNode* term, ScopeEntry* scope) {
  for_each(i, scope) {
    if (strcmp(term->term.name, scope[i].name) == 0) {
      return scope->type;
    }
  }
  handle(term, "Could not find in scope");
  return (Type) { .kind = -1 }; // Unreachable
}

static inline bool type_eq(Type type1, Type type2) {
  if (type1.kind == VAR && type2.kind == VAR || type1.kind == CONCRETE && type2.kind == CONCRETE)
    return type1.kind == VAR? type1.var.identifier == type2.var.identifier : type1.concrete == type2.concrete;
  if (type1.kind == CONSTRUCTOR && type2.kind == CONSTRUCTOR || type1.kind == FUNC && type2.kind == FUNC) {
    Type* vec1 = type1.constructor;
    Type* vec2 = type2.constructor;
    if (sizeof_vector(vec1) != sizeof_vector(vec2))
      return false;
    for_each(i, vec1) {
      if (!type_eq(vec1[i], vec2[i]))
        return false;
    }
    return true;
  }
  return false;
}

static inline bool is_constraint_subset(ASTNode* subset, ASTNode* set) {
  if (subset) for_each(i, subset) {
    if (set) for_each(j, set) {
      if (strcmp(subset[i].term.name, set[i].term.name) == 0)
        goto to_continue;
    }
    return false;
  to_continue:
    continue;
  }
  return true;
}

static inline bool type_iso(Type type1, Type type2) {
  if (type1.kind == CONCRETE && type2.kind == CONCRETE) {
    return type1.concrete == type2.concrete;
  }
  if (type1.kind == CONSTRUCTOR && type2.kind == CONSTRUCTOR || type1.kind == FUNC && type2.kind == FUNC) {
    Type* vec1 = type1.constructor; // .func ~ .constructor
    Type* vec2 = type2.constructor; //
    if (sizeof_vector(vec1) != sizeof_vector(vec2))
      return false;
    for_each(i, vec1) {
      if (!type_iso(vec1[i], vec2[i]))
        return false;
    }
    return true;
  }
  if (type1.kind == VAR) {
    switch (type2.kind) {
      case FUNC:
        return false;
      case CONSTRUCTOR: {
        Type constructor = type2.constructor[0];
        Instances instances = constructor.concrete;
        return is_constraint_subset(type1.var.constraints, instances);
        break;
      }
      case CONCRETE: {
        Instances instances = type2.concrete;
        return is_constraint_subset(type1.var.constraints, instances);
        break;
      }
      case VAR:
        return is_constraint_subset(type1.var.constraints, type2.var.constraints);
        break;
      case ANY:
        __builtin_unreachable();
    }
  }
  return false;
}

static inline void solve_func_equation(
  ASTNode* func_node, Type* func_type, Type* arg_type,
  unsigned int arg_position
) {
  if (func_type->func.func_type[arg_position].kind == ANY) {
    func_type->func.func_type[arg_position] = *arg_type;
    return;
  }
  if (arg_type->kind == ANY) {
    *arg_type = func_type->func.func_type[arg_position];
    return;
  } 

  if (!type_iso(*func_type, *arg_type)) {
    handle(func_node, "Type mismatch");
  }
  
  if (arg_type->kind == VAR && (func_type->func.func_type[arg_position].kind == CONCRETE || func_type->func.func_type[arg_position].kind == CONSTRUCTOR)) {
    InstanceEntry* instances;
  }

  for_each(i, func_type->func.func_type) {
    if (i != arg_position && type_eq(func_type->func.func_type[i], func_type->func.func_type[arg_position])) {
      func_type->func.func_type[i] = *arg_type;
    }
  }
  func_type->func.func_type[arg_position] = *arg_type;
}

static Type* get_type(ASTNode* node, ScopeEntry* scope_stack) {
  for_each(i, scope_stack) {
    if (strcmp(scope_stack[i].name, node->term.name) == 0) {
      return &scope_stack[i].type;
    }
  }
  Type* t = (Type*) follow_pattern_with_default(node->term.name, type_trie, 0);
  if (!t) {
    handle(node, "Could not find function");
  }
  return t;
}

static inline ASTNode* get_constraints(ASTNode* var, ASTNode** constraints) {
  if (constraints) for_each(i, constraints) {
    if (strcmp(constraints[i][0].term.name, var->term.name))
      return constraints[i] + 1;
  }
  return NULL;
}

static inline Type as_type(
  ASTNode type_node, ASTNode** type_constraints_context, int* type_identifier_context
) {
  if (type_node.type == EXPRESSION) {
    Type* type_v = new_vector_with_capacity(*type_v, sizeof_vector(type_node.expression_v));
    for_each(i, type_node.expression_v) {
      push(type_v, as_type(type_node.expression_v[i], type_constraints_context, type_identifier_context));
    }
    // The union member for constructor and func is identical
    // thus the observation that .func = ... <==> .constructor = ...
    return (Type) { .kind = type_node.expression_v[0].type == TYPE_CONSTRUCTOR? CONSTRUCTOR : FUNC, .func = type_v };
  }
  if (type_node.term.type == FUNCTION)
    return (Type) { .kind = VAR, .var.identifier = (*type_identifier_context)++, .var.constraints = get_constraints(&type_node, type_constraints_context)};
  ASTNode* conc;
  if (!(conc = (ASTNode*) follow_pattern_with_default(type_node.term.name, type_trie, 0))) {
    handle(&type_node, "Could not find type");
  }
  return (Type) { .kind = CONCRETE, .concrete = conc };
}

static inline Type constructor_to_type(ASTNode* constructor_node, ASTNode constructs) {
  Type* constructor_type = new_vector_with_capacity(*constructor_type, 4);
  int ctx = 0;
  for (unsigned long i = 1; i < sizeof_vector(constructor_node); i++) {
    push(constructor_type, as_type(constructor_node[i], NULL, &ctx));
  }
  push(constructor_type, as_type(constructs, NULL, &ctx));
  return (Type) { .kind = FUNC, .func = constructor_type };
}

static Type* type_of_expression(ASTNode* expression, ScopeEntry* scope, int* identifier_context) {
  switch (expression->type) {
    case BIN_EXPRESSION: {
      Type* targ;
      Type* op_signature = get_type(expression->bin_expression.op, scope);
      targ = type_of_expression(expression->bin_expression.left_expression_v, scope, identifier_context);
      solve_func_equation(expression->bin_expression.op, op_signature, targ, 0);
      targ = type_of_expression(expression->bin_expression.right_expression_v, scope, identifier_context);
      solve_func_equation(expression->bin_expression.op, op_signature, targ, 1);
      return &op_signature->func.func_type[2];
    }
    case EXPRESSION: {
      Type* targ;
      Type* f_signature = type_of_expression(expression->expression_v, scope, identifier_context);
      if (sizeof_vector(f_signature->func.func_type) != sizeof_vector(expression->expression_v)) {
        handle(expression->expression_v, "N-arity mismatch");
      }
      for (unsigned long i = 1; i < sizeof_vector(f_signature->func.func_type); i++) {
        targ = type_of_expression(expression->expression_v + i, scope, identifier_context);
        solve_func_equation(expression->bin_expression.op, f_signature, targ, i);
      }
      return &vector_last(f_signature->func.func_type);
    }
    case TERM: {
      switch (expression->term.type) {
        case FUNCTION:
        case TYPE_CONSTRUCTOR: return get_type(expression, scope);
        case TNATURAL:         return &_Chompa_Builtin_Natural;
        case TCHARACTER:       return &_Chompa_Builtin_Character;
        case TREAL:            return &_Chompa_Builtin_Real;
        case TSTRING:          return &_Chompa_Builtin_String;
      }
    }
    case V_DEFINITION: {
      Type* expression_type = type_of_expression(expression->variable_definition.expression, scope, identifier_context);
      push(scope_stack, ((ScopeEntry) { .name = expression->variable_definition.name->term.name, .type = *expression_type }));
      return expression_type;
    }
    case TYPE_ASSERTION: {
      ASTNode asserted_type_node = (ASTNode) { .type = EXPRESSION, .expression_v = expression->type_assertion.type_v };
      Type asserted_type = as_type(asserted_type_node, expression->type_assertion.constraints, identifier_context);
      Type* actual_type = type_of_expression(expression->type_assertion.expression, scope, identifier_context);
      if (!type_iso(*actual_type, asserted_type)) {
        handle(expression->type_assertion.type_v, "Asserted type does not match actual type");
      }
      return actual_type;
    }
    // These are top level and thus cannot occur in expressions
    case IMPLEMENTATION:
    case DATA_DECLARATION:
    case INSTANCE_DEFINITION:
    case CLASS_DECLARATION:
      __builtin_unreachable();
  }
}

bool checker(ASTNode* ast, Error* error_buf) {

  error_buf = error_buf;
  funcs_trie = create_node(0, -1);
  type_trie = create_node(0, -1);
  instance_trie = create_node(0, -1);
  class_trie = create_node(0, -1);

  scope_stack = new_vector_with_capacity(*scope_stack, 4);

  ASTNode node = ast[0];
  for_each_element(node, ast) {
    setjmp(jumping_buf);
    switch (node.type) {
      case V_DEFINITION: {
        int ctx = 0;
        Type* var_type = type_of_expression(node.variable_definition.expression, NULL, &ctx);
        insert_trie(node.variable_definition.name->term.name, (unsigned long) var_type, funcs_trie);
        break;
      }
      case TYPE_ASSERTION: {
        ASTNode type_node_expr = (ASTNode) { .type = EXPRESSION, .expression_v = ast->type_assertion.type_v };
        int ctx = 0;
        Type* type_func = (Type*) follow_pattern_with_default(ast->type_assertion.expression->term.name, funcs_trie, 0);
        Type type_func_expr = (Type) { .kind = FUNC, .func = type_func };
        if (!type_func) {
          insert_trie(ast->type_assertion.expression->term.name, (unsigned long) type_func, funcs_trie);
        } else if (!type_iso(type_func_expr, as_type(type_node_expr, ast->type_assertion.constraints, &ctx))) {
          handle(ast->type_assertion.type_v,  "Asserted type does not match actual type");
        }
        break;
      }
      case IMPLEMENTATION: {
        ScopeEntry* scope = new_vector_with_capacity(*scope, sizeof_vector(node.implementation.lhs));
        for (int i = 0; i < sizeof_vector(node.implementation.lhs); i++) {
          push(scope, ((ScopeEntry) { .name = node.implementation.lhs[i].term.name, .type = (Type) { .kind = ANY } }));
        }
        int ctx = 0;
        Type expression_type;
        for_each(i, node.implementation.body_v) {
          expression_type = *type_of_expression(node.implementation.body_v[i], scope, &ctx);
        }

        Type* func_type = new_vector_with_capacity(*func_type, sizeof_vector(node.implementation.lhs));
        for_each(i, scope) {
          push(func_type, scope[i].type);
        }
        push(func_type, expression_type);
        Type* func_type_accum = (Type*) follow_pattern_with_default(node.implementation.lhs->term.name, type_trie, 0);
        if (!func_type_accum) {
          // warn
          insert_trie(node.implementation.lhs->term.name, (unsigned long) func_type_accum, funcs_trie);
        } else {
          Type func_type_expr = (Type) { .kind = FUNC, .func = func_type };
          Type func_type_accum_expr = (Type) { .kind = FUNC, .func = func_type_accum };
          if (!type_iso(func_type_accum_expr, func_type_expr)) {
            handle(ast->implementation.lhs, "Implementation's type do not match");
          }
        }
        break;
      }
      case DATA_DECLARATION: {
        if (follow_pattern_with_default(node.data_declaration.type->term.name, type_trie, 0)) {
          handle(node.data_declaration.type, "Could not find type");
        }
        insert_trie(node.data_declaration.type->term.name, (unsigned long) node.data_declaration.type, type_trie);
        for_each(i, node.data_declaration.constructors) {
          Type constructor_type = constructor_to_type(node.data_declaration.constructors[i], *node.data_declaration.type);
          insert_trie(node.data_declaration.constructors[i]->term.name, (unsigned long) node.data_declaration.constructors[i], funcs_trie);
        }
        break;
      }
      case INSTANCE_DEFINITION: {
        ScopeEntry* class_declarations = (ScopeEntry*) follow_pattern_with_default(node.instance_definition.instance_class->term.name, class_trie, 0);
        if (!class_declarations) {
          handle(node.instance_definition.instance_class, "Could not find class");
        }
        InstanceEntry* entries = new_vector_with_capacity(*entries, sizeof_vector(class_declarations));
        unsigned long implementing_index = 0;
        InstanceEntry implementations = (InstanceEntry) { .name = class_declarations[0].name, .implementations = new_vector_with_capacity(*implementations.implementations, 4)};
        Type implementing_type = class_declarations[i].type;
        ASTNode* implementation = node.instance_definition.implementations_v[0];
        for_each_element(implementation, node.instance_definition.implementations_v) {
          // TODO: make this agnostic to order (right now we demand that declarations and implementations match order)
          if (strcmp(implementations.name, implementation->implementation.lhs->term.name) != 0) {
            implementing_index++;
            implementations = (InstanceEntry) { .name = class_declarations[i].name, .implementations = new_vector_with_capacity(*implementations.implementations, 4)};
          }
          push(implementations.implementations, *implementation);
        }
        insert_trie(node.instance_definition.instance_type->term.name, (unsigned long) entries, instance_trie);
        break;
      }
      case CLASS_DECLARATION: {
        if (follow_pattern_with_default(node.class_declaration.class_name->term.name, class_trie, 0)) {
          handle(node.class_declaration.class_name, "Could not find class");
        }
        Type* declarations_type_v = new_vector_with_capacity(*declarations_type_v, sizeof_vector(node.class_declaration.declarations_v));
        int ctx = 0;
        for_each(i, node.class_declaration.declarations_v) {
          // TODO: Add something to check declarations are unique
          Type* declaration_type = Malloc(sizeof(*declaration_type));
          ASTNode declaration_expression = (ASTNode) { .type = EXPRESSION, .expression_v = node.class_declaration.declarations_v[i] };
          push(declarations_type_v, *declaration_type = as_type(declaration_expression, NULL, &ctx));
          declaration_type->func.from_class = true;
          insert_trie(node.class_declaration.declarations_v[i]->implementation.lhs->term.name, (unsigned long) declaration_type, funcs_trie);
        }
        insert_trie(node.class_declaration.class_name->term.name, (unsigned long) declarations_type_v, class_trie);
        ScopeEntry* instances = new_vector_with_capacity(*instances, 8);
        insert_trie(node.class_declaration.class_name->term.name, (unsigned long) instances, instance_trie);
        break;
      }
      case TERM:
      case EXPRESSION:
      case BIN_EXPRESSION:
        __builtin_unreachable();
    }
  }

  return passes_type_check;
}
