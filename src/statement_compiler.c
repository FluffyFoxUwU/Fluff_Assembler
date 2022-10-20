#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include "hashmap.h"
#include "parser_stage2.h"
#include "parser_stage1.h"
#include "code_emitter.h"
#include "hashmap.h"
#include "statement_compiler.h"

struct statement_compiler* statement_compiler_new(struct parser_stage2* parser, struct statement_compiler* overrideBy) {
  if (parser && 
     overrideBy && overrideBy->parser && 
     overrideBy->parser != parser) {
    return NULL;
  }
  
  struct statement_compiler* self = malloc(sizeof(*self));
  if (!self)
    return NULL;
  self->overrideBy = overrideBy;
  self->parser = parser;
  hashmap_init(&self->emitterRegistry, hashmap_hash_string, strcmp);
  
  return self;
}

void statement_compiler_free(struct statement_compiler* self) {
  const char* k;
  struct emitter_func_entry* v;
  hashmap_foreach(k, v, &self->emitterRegistry)
    free(v);
  hashmap_cleanup(&self->emitterRegistry);
  
  if (!self)
    return;
  free(self);
}

int statement_compile(struct statement_compiler* self, struct parser_stage2_context* context, uint8_t cond, struct statement* statement) {
  struct emitter_func_entry* func = hashmap_get(&self->emitterRegistry, statement->wholeStatement.data[0]->data.identifier->data);
  if (!func)
    return -EADDRNOTAVAIL;
  
  uint16_t u16_a, u16_b, u16_c;
  uint32_t u32;
  int32_t s32;
  struct code_emitter_label* label;
  
  int res = 0;
  switch (func->type) {
    case EMITTER_NO_ARG:
      res = func->func.noArg(context->emitter, cond);
      break;
    case EMITTER_u16x1:
      res = func->func.u16x1(context->emitter, cond, u16_a);
      break;
    case EMITTER_u16x2:
      res = func->func.u16x2(context->emitter, cond, u16_a, u16_b);
      break;
    case EMITTER_u16x3:
      res = func->func.u16x3(context->emitter, cond, u16_a, u16_b, u16_c);
      break;
    case EMITTER_u16_u32:
      res = func->func.u16_u32(context->emitter, cond, u16_a, u32);
      break;
    case EMITTER_u16_s32:
      res = func->func.u16_s32(context->emitter, cond, u16_a, s32);
      break;
    case EMITTER_LABEL:
      res = func->func.label(context->emitter, cond, label);
      break;
    default:
      return -EINVAL;
  }
  return res;
}
