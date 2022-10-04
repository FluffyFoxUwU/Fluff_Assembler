#ifndef header_1662272744_52d075c5_7f9b_446c_9a2a_133655f3312d_bytecode_h
#define header_1662272744_52d075c5_7f9b_446c_9a2a_133655f3312d_bytecode_h

#include <stddef.h>
#include <stdint.h>

#include "vm_types.h"

#define BYTECODE_MAGIC ((uint64_t) 0x466F5855575500LL)

struct vm;

enum constant_type {
  BYTECODE_CONSTANT_INTEGER,
  BYTECODE_CONSTANT_NUMBER
};

struct constant {
  enum constant_type type;
  union {
    vm_int integer;
    vm_number number;
  } data;
};

struct bytecode {
  size_t constantsLen;
  struct constant* constants;

  size_t prototypesLen;
  struct prototype** prototypes;
};

struct bytecode* bytecode_new();

/* Errors:
 * -EINVAL: Too many prototypes
 * -ENOMEM: Not enough memory
 */
int bytecode_set_prototypes(struct bytecode* self, size_t prototypesLen, struct prototype** prototypes);

/* Errors:
 * -EINVAL: Too many constants
 * -ENOMEM: Not enough memory
 */
int bytecode_set_constants(struct bytecode* self, size_t constantsLen, struct constant* constants);

void bytecode_free(struct bytecode* self);

#endif

