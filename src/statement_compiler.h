#ifndef _headers_1666271700_Fluff_Assembler_statement_compiler
#define _headers_1666271700_Fluff_Assembler_statement_compiler

// The final piece bridging source parser to bytecode generator

#include "code_emitter.h"
#include "hashmap.h"

struct parser_stage2;
struct statement;
struct parser_stage2_context;

enum emitter_func_type {
  EMITTER_u16x3,
  EMITTER_u16x2,
  EMITTER_u16x1,
  EMITTER_u16_u32,
  EMITTER_u16_s32,
  EMITTER_LABEL,
  EMITTER_NO_ARG
};

struct emitter_func_entry {
  enum emitter_func_type type;
  union {
    code_emitter_u16x3_emitter_func u16x3;
    code_emitter_u16x2_emitter_func u16x2;
    code_emitter_u16x1_emitter_func u16x1;
    code_emitter_u16_u32_emitter_func u16_u32;
    code_emitter_u16_s32_emitter_func u16_s32;
    code_emitter_label_emitter_func label;
    code_emitter_no_arg_emitter_func noArg;
  } func;
};

struct statement_compiler {
  struct parser_stage2* parser;
  struct statement_compiler* overrideBy;
  HASHMAP(char, struct emitter_func_entry) emitterRegistry;
};

struct statement_compiler* statement_compiler_new(struct parser_stage2* parser, struct statement_compiler* overrideBy);
void statement_compiler_free(struct statement_compiler* self);

// Return 0 on success
// Errors:
// -EINVAL: Invalid state
// -ENOMEM: Not enough memory
// -EFAULT: Code emitter error
// -EADDRNOTAVAIL: No emitter for this instruction
int statement_compile(struct statement_compiler* self, struct parser_stage2_context* context, uint8_t cond, struct statement* statement);

#endif

