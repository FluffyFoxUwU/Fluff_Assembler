#ifndef _headers_1666271700_Fluff_Assembler_statement_compiler
#define _headers_1666271700_Fluff_Assembler_statement_compiler

// The final piece bridging source parser to bytecode generator

#include <stdbool.h>
#include <stdint.h>

#include "hashmap.h"

struct parser_stage2;
struct statement;
struct parser_stage2_context;

// enum emitter_func_type {
//   EMITTER_u16x3,
//   EMITTER_u16x2,
//   EMITTER_u16x1,
//   EMITTER_u16_u32,
//   EMITTER_u16_s32,
//   EMITTER_LABEL,
//   EMITTER_NO_ARG
// };

struct statement_processor;
struct statement_processor_context;
typedef int (*statement_processor_func)(struct statement_processor_context* ctx);

struct statement_processor {
  void* udata;
  int udata1;
  statement_processor_func processor;
  
  const char* name;
  struct statement_compiler* owner;
};

struct statement_compiler {
  struct parser_stage2* parser;
  struct statement_compiler* overrideBy;
  HASHMAP(char, struct statement_processor) emitterRegistry;
};

struct statement_processor_context {
  struct statement_compiler* owner;
  struct parser_stage2_context* stage2Context;
  struct statement_processor* funcEntry;
  struct statement* statement;
  struct token* instructionToken;
  
  bool canFreeErr; 
  const char* err;
};

struct statement_compiler* statement_compiler_new(struct parser_stage2* parser, struct statement_compiler* overrideBy);
void statement_compiler_free(struct statement_compiler* self);

// Return 0 on success
// Errors:
// -EINVAL: Invalid argument
// -ENOMEM: Not enough memory
// -EEXIST: An instruction with same name exists
int statement_compiler_register(struct statement_compiler* self, const char* name, struct statement_processor* entry);

// Return 0 on success
// Errors:
// -EINVAL: Invalid argument
// -EADDRNOTAVAIL: Instruction with given name doesnt exist
int statement_compiler_unregister(struct statement_compiler* self, const char* name);

// Return 0 on success
// Errors:
// -EINVAL: Invalid state
// -ENOMEM: Not enough memory
// -EFAULT: Compile error
// -EADDRNOTAVAIL: No emitter for this instruction
int statement_compile(struct statement_compiler* self, struct parser_stage2_context* context, struct statement* statement, bool* canFreeErr, char** err);

#endif

