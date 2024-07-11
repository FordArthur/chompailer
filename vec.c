#include "vec.h"

void* _new_vector(unsigned int obj_size, unsigned int capacity) {
  vect_h* vp = malloc( sizeof(vect_h) + obj_size * capacity );
  if (vp) {
    *vp = (vect_h){
      .obj_size = obj_size,
      .capacity = capacity,
    };
  }
  return vp + 1;
}

vect_h* _get_header(void* vect) {
  return ((vect_h*) vect) - 1;
}
