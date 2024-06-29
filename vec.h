#ifndef INSITUCS_VECTOR_HEADER
#define INSITUCS_VECTOR_HEADER

#include <stdlib.h> // may (?) cause problems

typedef struct Vector_Header {
  unsigned int obj_size;
  unsigned int capacity;
  unsigned int size;
} vect_h;

void* _new_vector(unsigned int obj_size, unsigned int capacity);
vect_h* _get_header(void* vect);

#define new_vector(T) _new_vector(sizeof(T), CAPACITY)
#define new_vector_with_capacity(T, c) _new_vector(sizeof(T), c)
#define CAPACITY 16

#define sizeof_vector(v) _get_header(v)->size
#define push(v, i) {\
  vect_h* header = _get_header(v);\
  v[header->size] = i;\
  header->size++;\
  if (header->size == header->capacity) {\
    header->capacity += CAPACITY;\
    header = realloc(header, sizeof(vect_h) + header->capacity * header->obj_size);\
    v = (void*) (header + 1);\
  }\
}\

#define for_each(i, v) for (unsigned int i = 0, m = _get_header(v)->size; i < m; i++)

#endif // !INSITUCS_VECTOR_HEADER
