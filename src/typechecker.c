#include "typechecker.h"
#include "parser.h"
#include "trie.h"
#include "vec.h"
#include <string.h>

static TrieNode* astrie;
static bool passes_type_check = true;
static Error* global_error_buf;
static TrieNode* type_trie;
static TrieNode* instance_trie;

// type_stack

static TypeConcrete** type_cache_accum;
static TypeConcrete** type_cache;

typedef struct ScopeEntry {
  char* name;
  TypeConcrete* type;
} ScopeEntry;

static ScopeEntry* scope_stack;


static inline void reuse_push(TypeConcrete** cache, bool total_vec_push, TypeConcrete type) {
  vect_h* cache_header = _get_header(cache);
  if (total_vec_push && cache_header->size == cache_header->capacity) {
    cache_header = realloc(cache_header, sizeof(vect_h) + sizeof(TypeConcrete*)*(cache_header->capacity += 4));
  }
  push(cache[total_vec_push? cache_header->size : cache_header->size - 1], type);
  if (total_vec_push) cache_header->size++;
}

static inline TypeConcrete astype_to_type(ASTNode type, signed short* ctx, char* string_rep) {
  if (type.term.type == TYPE) {
    TypeConcrete t;
    if (!(t = follow_pattern_with_default(type.term.name, type_trie, 0))) {
      // err
    }
    return t;
  } else {
    TypeVar t = (TypeVar) { .info = (*ctx)++, .ast_ptr = (unsigned long) string_rep & 0x0000ffffffffffff };
    return *(TypeConcrete*) &t;
  }
}

static void type_of_types(ASTNode** expr, signed short* ctx, TypeConcrete** cache) {
  ASTNode* constructor = expr[0];
  for_each_element(constructor, expr) {
    ASTNode term = constructor[0];
    bool is_first_push = true;
    for_each_element(term, constructor) {
      reuse_push(cache, is_first_push, astype_to_type(term, ctx, term.term.name));
      is_first_push = false;
    }
  }
}

static inline TypeConcrete* get_signature(char* func, ScopeEntry* scope) {
  for_each(i, scope) if (strcmp(func, scope[i].name)) return scope[i].type;
  return ((ASTNode*) follow_pattern_with_default(func, astrie, NULL))->function_definition.declaration;
}

static inline bool type_() {

}

static void type_of_expression(ScopeEntry* scope, ASTNode* expression, TypeConcrete** cache) {
  if (expression->type == BIN_EXPRESSION) {
    TypeConcrete* op_signature = get_signature(expression->bin_expression.op->term.name, scope);
    type_of_expression(scope, expression->bin_expression.left_expression_v, cache);
    type_of_expression(scope, expression->bin_expression.right_expression_v, cache);
  } else {

  }
}

static void type_of_implementation(ASTNode* args, ASTNode** body_v, signed short* ctx, TypeConcrete** cache) {
  for (unsigned long i = 0; i <= _get_header(args)->size; i++, (push(scope_stack, ((ScopeEntry) { .name = args->term.name, .type = vector_last(cache) } ) ))) {
    TypeVar t = (TypeVar) { .info = (*ctx)++, .ast_ptr = 0 };
    reuse_push(cache, true, *(TypeConcrete*) &t);
  }

  ASTNode* expression = body_v[0];
  for_each_element(expression, body_v) {
  }

  vector_empty_it(scope_stack);
}

static void check(unsigned long _def) {
  ASTNode* def = (ASTNode*) _def;
  signed short ctx = 0x8000;
  if (def->function_definition.declaration) 
    type_of_types(def->function_definition.declaration->formal_type.type_v_v, &ctx, type_cache_accum);
  else type_of_implementation(def->function_definition.implementations_v->implementation.arguments_v,
                              def->function_definition.implementations_v->implementation.body_v,
                              &ctx, type_cache_accum);

  ASTNode impl = def->function_definition.declaration? def->function_definition.implementations_v[0] : def->function_definition.implementations_v[1];
  for_each_element(impl, def->function_definition.implementations_v) {
    bool matches = true;
    type_of_implementation(impl.implementation.arguments_v, impl.implementation.body_v, &ctx, type_cache);
    for_each(i, type_cache_accum) {
      for_each(j, type_cache_accum[i]) {
        matches &= type_eq(type_cache_accum[i][j], type_cache[i][j]);
      }
    }
    if (!matches) {
      passes_type_check = false;
      // err
    }
  }
  vector_empty_it(type_cache_accum);
  vector_empty_it(type_cache);
}

bool checker(TrieNode* _astrie, TrieNode* _type_trie, TrieNode* _instance_trie, Error* error_buf) {
  _Static_assert(sizeof(TypeVar) == sizeof(TypeConcrete), "Type and TypeVar must have the same size");

  global_error_buf = error_buf;
  astrie  = _astrie;
  type_trie = _type_trie;
  instance_trie = _instance_trie;

  // Initialize class table & predefined types
  // ...

  type_cache_accum = new_vector_with_capacity(*type_cache_accum, 4);
  for (unsigned char i = 0; i < 4; i++)
    type_cache_accum[i] = new_vector_with_capacity(*type_cache_accum[i], 4);

  type_cache = new_vector_with_capacity(*type_cache, 4);
  for (unsigned char i = 0; i < 4; i++)
    type_cache[i] = new_vector_with_capacity(*type_cache[i], 4);

  scope_stack = new_vector_with_capacity(*scope_stack, 4);

  for_each_in_trie(_astrie, check);
  return passes_type_check;
}


/** IDEAS:
 * - DONE: instead of bitfield, have signed short (the signed bit is turned on iff its a type var, and when it is it's leaving us exactly with 2^15 range, easy to test and to use)
 * - Cache Type* too
 */
/** TODO:
 * - `check` disables values temporarly, keeping the value but setting the boolean to false
 * - `check` should check whether it's a function or constructor definition
 */
