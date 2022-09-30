#include <stdint.h>
#include <stdlib.h>
#include <Block.h>
#include <errno.h>

#include "code_emitter.h"
#include "vm_types.h"
#include "opcodes.h"

struct code_emitter* code_emitter_new() {
  struct code_emitter* self = malloc(sizeof(*self));
  
  self->instructionCount = 0;
  self->instructions = NULL;
  self->partialInstructions = NULL;
  self->labelCount = 0;
  self->labels = NULL;
  self->ip = 0;

  return self;
}

struct code_emitter_label* code_emitter_label_new(struct code_emitter* self, struct token token) {
  self->labels = realloc(self->labels, sizeof(*self->labels) * (self->labelCount + 1));
  if (!self->labels)
    return NULL;

  self->labelCount++;

  struct code_emitter_label* label = &self->labels[self->labelCount - 1];
  label->defined = false;
  label->definedAt = token;
  label->location = 0;
  label->owner = self;
  return label;
}

int code_emitter_label_define(struct code_emitter* self, struct code_emitter_label* label) {
  if (label->defined || label->owner != self)
    return -EINVAL;

  label->defined = true;
  label->location = self->ip;
  return 0;
}

void code_emitter_free(struct code_emitter* self) {
  for (vm_instruction_pointer i = 0; i < self->instructionCount; i++) {
    if (self->partialInstructions[i].type == INSTRUCTION_DEFERRED) {
      self->partialInstructions[i].data.deferred();
      Block_release(self->partialInstructions[i].data.deferred);
    }
  }

  free(self->partialInstructions);
  free(self->instructions);
  free(self);
}

static int emit(struct code_emitter* self, struct instruction ins) {
  self->partialInstructions = realloc(self->partialInstructions, sizeof(*self->partialInstructions) * (self->instructionCount + 1));
  if (!self->partialInstructions)
    return -ENOMEM;

  self->instructionCount++;
  self->partialInstructions[self->instructionCount - 1] = ins;
  return 0;
}

int code_emitter_emit_jmp(struct code_emitter* self, int cond, struct code_emitter_label* target) {
  vm_instruction_pointer current = self->ip;
  struct instruction ins = {
    .type = INSTRUCTION_DEFERRED,
    .data.deferred = Block_copy(^vm_instruction () {
      // Shouldn't be any undefined addresses
      if (!target->defined)
        abort();
      
      vm_instruction_pointer targetIP = target->location;
      int op = FLUFFYVM_OPCODE_JMP_BACKWARD;
      vm_instruction_pointer delta;
      if (targetIP > current) {
        delta = targetIP - current;
        op = FLUFFYVM_OPCODE_JMP_FORWARD;
      } else {
        delta = current - targetIP;
      }
      vm_instruction instruction = OP_ARG_OPCODE(op) |
        OP_ARG_COND(cond) |
        OP_ARG_A_U32(delta);

      printf("delta yay:  %016" PRINT_VM_INSTRUCTION_POINTER_X "\n", delta);
      printf("yay:  %016" PRIx64 "\n", instruction);
      return instruction;
    })
  };

  if (ins.data.deferred == NULL)
    return -ENOMEM;

  self->ip++;
  return emit(self, ins);
}


