#ifndef header_1664459967_36f09eef_2f1a_45a5_a744_a3e16cfaddd9_code_emitter_h
#define header_1664459967_36f09eef_2f1a_45a5_a744_a3e16cfaddd9_code_emitter_h

#include <stdbool.h>

#include "vec.h"
#include "vm_types.h"
#include "lexer.h"

enum instruction_type {
  INSTRUCTION_DEFERRED,
  INSTRUCTION_NORMAL
};

struct instruction {
  enum instruction_type type;

  union {
    vm_instruction instruction;
    vm_instruction (^deferred)(bool* status);
  } data;
};

struct code_emitter;
struct code_emitter_label {
  struct code_emitter* owner;

  bool defined;
  vm_instruction_pointer location;

  struct token* definedAt;
  
  // Number of time label is used
  int usageCount;
};

struct code_emitter {
  bool finalized;
  
  vec_t(struct code_emitter_label*) labels;
  vec_t(struct instruction) partialInstructions;
  vec_t(vm_instruction) instructions;
  
  vm_instruction_pointer ip;
  
  bool shuttingDown;
  bool canFreeErrorMessage;
  const char* errorMessage;
};

struct code_emitter* code_emitter_new();
void code_emitter_free(struct code_emitter* self);

// Finalize and generate code
// 0 on success
// Errors:
// -ENOMEM: Not enough memory to generate
// -EFAULT: Finalization failure (check self->errorMessage)
// -EINVAL: Finalize a fully or partially finalized code emitter
int code_emitter_finalize(struct code_emitter* self);

struct code_emitter_label* code_emitter_label_new(struct code_emitter* self, struct token* token);

/* Return 0 on success
 * Errors:
 * -EINVAL: Defining on wrong code emitter or
 *          attempt to double define
 */
int code_emitter_label_define(struct code_emitter* self, struct code_emitter_label* label);

/* Emitters for each instruction
 * 0 on success
 * Errors:
 * -ENOMEM: Not enough memory
 * -ENOSPC: Number of instructions is exceeding VM_LIMIT_MAX_CODE
 */ 
int code_emitter_emit_jmp(struct code_emitter* self, uint8_t cond, struct code_emitter_label* target);

typedef int (*code_emitter_u16x3_emitter_func)(struct code_emitter* self, uint8_t cond, uint16_t a, uint16_t b, uint16_t c);
typedef int (*code_emitter_u16x2_emitter_func)(struct code_emitter* self, uint8_t cond, uint16_t a, uint16_t b);
typedef int (*code_emitter_u16x1_emitter_func)(struct code_emitter* self, uint8_t cond, uint16_t a);
typedef int (*code_emitter_u16_u32_emitter_func)(struct code_emitter* self, uint8_t cond, uint16_t a, uint32_t b);
typedef int (*code_emitter_u16_s32_emitter_func)(struct code_emitter* self, uint8_t cond, uint16_t a, int32_t b);
typedef int (*code_emitter_label_emitter_func)(struct code_emitter* self, uint8_t cond, struct code_emitter_label* label);
typedef int (*code_emitter_no_arg_emitter_func)(struct code_emitter* self, uint8_t cond);

// Empty? Well, there no instruction taking one u16, yet
#define CODE_EMITTER_U16x1_INSTRUCTIONS

#define CODE_EMITTER_U16x3_INSTRUCTIONS \
  X(add, FLUFFYVM_OPCODE_ADD) \
  X(sub, FLUFFYVM_OPCODE_SUB) \
  X(mul, FLUFFYVM_OPCODE_MUL) \
  X(div, FLUFFYVM_OPCODE_DIV) \
  X(mod, FLUFFYVM_OPCODE_MOD) \
  X(pow, FLUFFYVM_OPCODE_POW) \
  X(impldep3, FLUFFYVM_OPCODE_IMPLDEP3) \
  X(impldep4, FLUFFYVM_OPCODE_IMPLDEP4) \
  X(get_array, FLUFFYVM_OPCODE_GET_ARRAY) \
  X(set_array, FLUFFYVM_OPCODE_SET_ARRAY) \

#define CODE_EMITTER_U16x2_INSTRUCTIONS \
  X(mov, FLUFFYVM_OPCODE_MOV) \
  X(cmp, FLUFFYVM_OPCODE_CMP) \

#define CODE_EMITTER_U16_S32_INSTRUCTIONS \
  X(ldint, FLUFFYVM_OPCODE_LOAD_INTEGER) \
  
#define CODE_EMITTER_U16_U32_INSTRUCTIONS \
  X(ldconst, FLUFFYVM_OPCODE_LOAD_CONSTANT) \
  X(impldep1, FLUFFYVM_OPCODE_IMPLDEP1) \
  X(impldep2, FLUFFYVM_OPCODE_IMPLDEP2) \
  X(ldproto, FLUFFYVM_OPCODE_LOAD_PROTOTYPE) \
  X(new_array, FLUFFYVM_OPCODE_NEW_ARRAY) \

#define CODE_EMITTER_NO_ARG_INSTRUCTIONS \
  X(nop, FLUFFYVM_OPCODE_NOP) \
  X(ret, FLUFFYVM_OPCODE_RET) \

#define X(name, op) \
  int code_emitter_emit_ ## name(struct code_emitter* self, uint8_t cond, uint16_t a, uint16_t b);
CODE_EMITTER_U16x2_INSTRUCTIONS
#undef X

#define X(name, op) \
  int code_emitter_emit_ ## name(struct code_emitter* self, uint8_t cond, uint16_t a, uint32_t b);
CODE_EMITTER_U16_U32_INSTRUCTIONS
#undef X

#define X(name, op) \
  int code_emitter_emit_ ## name(struct code_emitter* self, uint8_t cond, uint16_t a);
CODE_EMITTER_U16x1_INSTRUCTIONS
#undef X

#define X(name, op) \
  int code_emitter_emit_ ## name(struct code_emitter* self, uint8_t cond, uint16_t a, int32_t b);
CODE_EMITTER_U16_S32_INSTRUCTIONS
#undef X

#define X(name, op) \
  int code_emitter_emit_ ## name(struct code_emitter* self, uint8_t cond, uint16_t a, uint16_t b, uint16_t c);
CODE_EMITTER_U16x3_INSTRUCTIONS
#undef X

#define X(name, op) \
  int code_emitter_emit_ ## name(struct code_emitter* self, uint8_t cond);
CODE_EMITTER_NO_ARG_INSTRUCTIONS
#undef X

#endif

