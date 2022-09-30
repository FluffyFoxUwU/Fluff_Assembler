#ifndef header_1664459967_36f09eef_2f1a_45a5_a744_a3e16cfaddd9_code_emitter_h
#define header_1664459967_36f09eef_2f1a_45a5_a744_a3e16cfaddd9_code_emitter_h

#include <stdbool.h>

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
    vm_instruction (^deferred)();
  } data;
};

struct code_emitter;
struct code_emitter_label {
  struct code_emitter* owner;

  bool defined;
  vm_instruction_pointer location;

  struct token definedAt;
};

struct code_emitter {
  size_t labelCount;
  struct code_emitter_label* labels;

  vm_instruction_pointer instructionCount;
  struct instruction* partialInstructions;
  vm_instruction* instructions;
  
  vm_instruction_pointer ip;
};

struct code_emitter* code_emitter_new();
void code_emitter_free(struct code_emitter* self);

struct code_emitter_label* code_emitter_label_new(struct code_emitter* self, struct token token);

/* Return 0 on success
 * Errors:
 * -EINVAL: Defining on wrong code emitter or
 *          attempt to double define
 */
int code_emitter_label_define(struct code_emitter* self, struct code_emitter_label* label);

// Emitters
int code_emitter_emit_jmp(struct code_emitter* self, int cond, struct code_emitter_label* target);

#endif

