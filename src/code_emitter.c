#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <Block.h>
#include <errno.h>

#include "code_emitter.h"
#include "common.h"
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
  self->finalized = false;
  self->errorMessage = NULL;
  self->canFreeErrorMessage = false;
  self->shuttingDown = false;

  return self;
}

void code_emitter_free(struct code_emitter* self) {
  self->shuttingDown = true;
  
  bool status = true;
  for (vm_instruction_pointer i = 0; i < self->instructionCount; i++) {
    if (self->partialInstructions[i].type == INSTRUCTION_DEFERRED && 
        self->partialInstructions[i].data.deferred) {
      self->partialInstructions[i].data.deferred(&status);
      Block_release(self->partialInstructions[i].data.deferred);
    }
  }
  
  if (self->canFreeErrorMessage)
    free((char*) self->errorMessage);

  free(self->partialInstructions);
  free(self->instructions);
  free(self);
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

static int emit(struct code_emitter* self, struct instruction ins) {
  self->partialInstructions = realloc(self->partialInstructions, sizeof(*self->partialInstructions) * (self->instructionCount + 1));
  if (!self->partialInstructions)
    return -ENOMEM;

  self->instructionCount++;
  self->partialInstructions[self->instructionCount - 1] = ins;
  return 0;
}

int code_emitter_finalize(struct code_emitter* self) {
  if (self->finalized)
    return -EINVAL;
  self->finalized = true;

  vm_instruction* instructions = calloc(self->instructionCount, sizeof(*self->instructions));
  if (!instructions)
    return -ENOMEM;
  
  for (vm_instruction_pointer i = 0; i < self->instructionCount; i++) {
    struct instruction* ins = &self->partialInstructions[i];
    bool res = true;
    switch (ins->type) {
      case INSTRUCTION_NORMAL:
        instructions[i] = ins->data.instruction;
        break;
      case INSTRUCTION_DEFERRED:
        instructions[i] = ins->data.deferred(&res);
        Block_release(ins->data.deferred);
        ins->data.deferred = NULL;
        if (!res) {
          free(instructions);
          return -EFAULT;
        }
        break; 
    }
  }
  
  self->instructions = instructions;
  return 0;
}

static void setErrorMessage(struct code_emitter* self, const char* filename, int line, int column, const char* fmt, ...) {
  if (self->shuttingDown)
    return;
  
  va_list args;
  va_start(args, fmt);
  char* errmsg = common_format_error_message_valist(filename, "code emitter", line, column, fmt, args);
  va_end(args);
  
  if (!errmsg) {
    self->canFreeErrorMessage = false;
    self->errorMessage = "Out of memory";
    return;
  }
  
  self->canFreeErrorMessage = true;
  self->errorMessage = errmsg;
}

int code_emitter_emit_jmp(struct code_emitter* self, uint8_t cond, struct code_emitter_label* target) {
  vm_instruction_pointer current = self->ip;
  struct instruction ins = {
    .type = INSTRUCTION_DEFERRED,
    .data.deferred = Block_copy(^vm_instruction (bool* status) {
      // Use of undefined labels
      if (!target->defined) {
        *status = false;
        setErrorMessage(self, target->definedAt.filename, target->definedAt.startLine, target->definedAt.startColumn, "Use of undefined label!");
        return 0;
      }
      
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
      
      return instruction;
    }) 
  };

  if (ins.data.deferred == NULL)
    return -ENOMEM;

  self->ip++;
  return emit(self, ins);
}

// Other instructions generation UwU
#define gen_u16x3(name, op) \
  int code_emitter_emit_ ## name(struct code_emitter* self, uint8_t cond, uint16_t a, uint16_t b, uint16_t c) { \
    self->ip++; \
    return emit(self, (struct instruction) { \
      .type = INSTRUCTION_NORMAL, \
      .data.instruction = OP_ARG_OPCODE(op) | \
        OP_ARG_COND(cond) | \
        OP_ARG_A_U16x3(a) | \
        OP_ARG_B_U16x3(b) | \
        OP_ARG_C_U16x3(c) \
    }); \
  }
#define gen_u16x2(name, op) \
  int code_emitter_emit_ ## name(struct code_emitter* self, uint8_t cond, uint16_t a, uint16_t b) { \
    self->ip++; \
    return emit(self, (struct instruction) { \
      .type = INSTRUCTION_NORMAL, \
      .data.instruction = OP_ARG_OPCODE(op) | \
        OP_ARG_COND(cond) | \
        OP_ARG_A_U16x3(a) | \
        OP_ARG_B_U16x3(b) \
    }); \
  }
#define gen_u16_u32(name, op) \
  int code_emitter_emit_ ## name(struct code_emitter* self, uint8_t cond, uint16_t a, uint32_t b) { \
    self->ip++; \
    return emit(self, (struct instruction) { \
      .type = INSTRUCTION_NORMAL, \
      .data.instruction = OP_ARG_OPCODE(op) | \
        OP_ARG_COND(cond) | \
        OP_ARG_A_U16_U32(a) | \
        OP_ARG_B_U16_U32(b) \
    }); \
  }
#define gen_u16_s32(name, op) \
  int code_emitter_emit_ ## name(struct code_emitter* self, uint8_t cond, uint16_t a, int32_t b) { \
    self->ip++; \
    return emit(self, (struct instruction) { \
      .type = INSTRUCTION_NORMAL, \
      .data.instruction = OP_ARG_OPCODE(op) | \
        OP_ARG_COND(cond) | \
        OP_ARG_A_U16_S32(a) | \
        OP_ARG_B_U16_S32(b) \
    }); \
  }
#define gen_no_arg(name, op) \
  int code_emitter_emit_ ## name(struct code_emitter* self, uint8_t cond) { \
    self->ip++; \
    return emit(self, (struct instruction) { \
      .type = INSTRUCTION_NORMAL, \
      .data.instruction = OP_ARG_OPCODE(op) | \
        OP_ARG_COND(cond) \
    }); \
  }

#define X(name, op) gen_u16x2(name, op);
CODE_EMITTER_U16x2_INSTRUCTIONS
#undef X

#define X(name, op) gen_u16_u32(name, op);
CODE_EMITTER_U16_U32_INSTRUCTIONS
#undef X

#define X(name, op) gen_u16_s32(name, op);
CODE_EMITTER_U16_S32_INSTRUCTIONS
#undef X

#define X(name, op) gen_u16x3(name, op);
CODE_EMITTER_U16x3_INSTRUCTIONS
#undef X

#define X(name, op) gen_no_arg(name, op);
CODE_EMITTER_NO_ARG_INSTRUCTIONS
#undef X
