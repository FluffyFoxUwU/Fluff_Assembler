#ifndef header_1662272744_52d075c5_7f9b_446c_9a2a_133655f3312d_bytecode_h
#define header_1662272744_52d075c5_7f9b_446c_9a2a_133655f3312d_bytecode_h

#include <stddef.h>
#include <stdint.h>

#include "bytecode/prototype.h"
#include "vec.h"
#include "vm_types.h"

#define BYTECODE_MAGIC ((uint64_t) 0x466F5855575500LL)

enum constant_type {
  BYTECODE_CONSTANT_INTEGER,
  BYTECODE_CONSTANT_NUMBER,
  BYTECODE_CONSTANT_STRING
};

struct constant {
  enum constant_type type;
  union {
    vm_int integer;
    vm_number number;
  } data;
};

struct bytecode {
  vec_t(struct constant) constants;
  struct prototype* mainPrototype;
};

struct bytecode* bytecode_new();
void bytecode_free(struct bytecode* self);

#endif

