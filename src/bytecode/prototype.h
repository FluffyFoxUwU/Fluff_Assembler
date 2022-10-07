#ifndef header_1662272978_47ee9894_beca_4e37_be7d_a1a8c4d5a3ac_prototype_h
#define header_1662272978_47ee9894_beca_4e37_be7d_a1a8c4d5a3ac_prototype_h

#include <stddef.h>

#include "vec.h"
#include "vm_types.h"

struct bytecode;

struct prototype {
  const char* sourceFile;
  const char* prototypeName;
  int definedAtLine;
  int definedAtColumn;
  
  vec_t(struct prototype*) prototypes;
  vec_t(vm_instruction) instructions;
};

struct prototype* prototype_new(const char* sourceFile, const char* prototypeName, int line, int column);
void prototype_free(struct prototype* self);

#endif

