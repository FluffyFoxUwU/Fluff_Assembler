#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <Block.h>
#include <errno.h>

#include "code_emitter.h"
#include "common.h"
#include "vm_types.h"
#include "opcodes.h"
#include "vm_limits.h"
#include "vec.h"

struct code_emitter* code_emitter_new() {
  struct code_emitter* self = malloc(sizeof(*self));
  
  self->ip = 0;
  self->finalized = false;
  self->errorMessage = NULL;
  self->canFreeErrorMessage = false;
  self->shuttingDown = false;
  
  vec_init(&self->labels);
  vec_init(&self->instructions);
  vec_init(&self->partialInstructions);
  return self;
}

void code_emitter_free(struct code_emitter* self) {
  self->shuttingDown = true;
  code_emitter_finalize(self);
  
  if (self->canFreeErrorMessage)
    free((char*) self->errorMessage);

  struct instruction* ins;
  int i = 0;
  vec_foreach_ptr(&self->partialInstructions, ins, i)
    if (ins->type == INSTRUCTION_DEFERRED)
      Block_release(ins->data.deferred);

  struct code_emitter_label* label = 0;
  vec_foreach(&self->labels, label, i)
    free(label);
  
  vec_deinit(&self->labels);
  vec_deinit(&self->partialInstructions);
  vec_deinit(&self->instructions);
  free(self);
}

struct code_emitter_label* code_emitter_label_new(struct code_emitter* self, struct token* token) {
  struct code_emitter_label* label = malloc(sizeof(*self));
  if (!label)
    return NULL;
  
  label->defined = false;
  label->definedAt = token;
  label->location = 0;
  label->owner = self;
  label->usageCount = 0; 
  
  if (vec_push(&self->labels, label) < 0)
    return NULL;
  return label;
}

int code_emitter_label_define(struct code_emitter* self, struct code_emitter_label* label) {
  if (label->defined || label->owner != self)
    return -EINVAL;

  label->defined = true;
  label->location = self->ip;
  return 0;
}

static inline int emit(struct code_emitter* self, struct instruction ins) {
  if (self->ip >= VM_LIMIT_MAX_CODE)
    return -ENOSPC;
  
  int ret = vec_push(&self->partialInstructions, ins) < 0 ? -ENOMEM : 0;
  self->ip++;
  return ret;
}

int code_emitter_finalize(struct code_emitter* self) {
  if (!self->shuttingDown && self->finalized)
    return -EINVAL;
  self->finalized = true;
  
  if (!self->shuttingDown && vec_reserve(&self->instructions, self->partialInstructions.length) < 0)
    return -ENOMEM;
   
  bool status = true;
  int res = 0;
  struct instruction* ins;
  int i = 0;
  vec_foreach_ptr(&self->partialInstructions, ins, i) {
    vm_instruction instructionFinalized;
    
    switch (ins->type) {
      case INSTRUCTION_NORMAL:
        instructionFinalized = ins->data.instruction;
        break;
      case INSTRUCTION_DEFERRED:
        if (!ins->data.deferred)
          continue;
        
        instructionFinalized = ins->data.deferred(&status);
        Block_release(ins->data.deferred);
        ins->data.deferred = NULL;
        
        if (!status) {
          res = -EFAULT;
          goto fail_deferred;
        }
        break; 
    }
    
    if (!self->shuttingDown && vec_insert(&self->instructions, i, instructionFinalized) < 0) { 
      res = -ENOMEM;
      goto fail_insert;
    }
  }
  
  fail_deferred:
  fail_insert:
  return res;
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
  target->usageCount++;
  
  vm_instruction_pointer current = self->ip;
  struct instruction ins = {
    .type = INSTRUCTION_DEFERRED,
    .data.deferred = Block_copy(^vm_instruction (bool* status) {
      // Use of undefined labels
      if (!target->defined) {
        *status = false;
        setErrorMessage(self, target->definedAt->filename, target->definedAt->startLine, target->definedAt->startColumn, "Use of undefined label!");
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

  return emit(self, ins);
}

// Other instructions generation UwU
#define gen_u16x3(name, op) \
  int code_emitter_emit_ ## name(struct code_emitter* self, uint8_t cond, uint16_t a, uint16_t b, uint16_t c) { \
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
    return emit(self, (struct instruction) { \
      .type = INSTRUCTION_NORMAL, \
      .data.instruction = OP_ARG_OPCODE(op) | \
        OP_ARG_COND(cond) | \
        OP_ARG_A_U16x3(a) | \
        OP_ARG_B_U16x3(b) \
    }); \
  }
#define gen_u16x1(name, op) \
  int code_emitter_emit_ ## name(struct code_emitter* self, uint8_t cond, uint16_t a) { \
    return emit(self, (struct instruction) { \
      .type = INSTRUCTION_NORMAL, \
      .data.instruction = OP_ARG_OPCODE(op) | \
        OP_ARG_COND(cond) | \
        OP_ARG_A_U16x3(a) \
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

#define X(name, op) gen_u16x1(name, op);
CODE_EMITTER_U16x1_INSTRUCTIONS
#undef X

#define X(name, op) gen_no_arg(name, op);
CODE_EMITTER_NO_ARG_INSTRUCTIONS
#undef X
