#ifndef _headers_1665895918_Fluff_Assembler_parser_stage2
#define _headers_1665895918_Fluff_Assembler_parser_stage2

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "buffer.h"
#include "vec.h"
#include "hashmap.h"

// Stage 2 parser (Process statements into final bytecode product
// which finally processed by protobuf to generate the data)

struct code_emitter_label;
struct code_emitter;
struct prototype;
struct statement_compiler;
struct bytecode;
struct parser_stage1;
struct token_iterator;
struct parser_stage2_context;

struct prototype_registry_entry {
  uint32_t id;
  uint32_t resolvedLocation;
  
  struct prototype* proto;
  const char* name;
};

struct parser_stage2 {
  struct parser_stage1* parser;
  struct statement_compiler* statementCompiler;
  
  bool isCompleted;
  bool canFreeErrorMsg;
  bool isFirstStatement;
  const char* errorMessage;
  
  const char* currentInputName;
  
  int nextStatementPointer; 
  struct statement* currentStatement;
  
  struct bytecode* bytecode;
  struct parser_stage2_context* currentCtx;
  
  bool isStatementCompilerRegistered;
};

struct parser_stage2_context {
  struct parser_stage2* owner;
  struct prototype* proto;
  struct code_emitter* emitter;
  
  HASHMAP(char, struct code_emitter_label) labelLookup;
  HASHMAP(char, struct prototype_registry_entry) prototypesRegistry;
  vec_t(struct prototype_registry_entry*) prototypesRegistryEntries;
  vec_t(struct statement*) ipToStamement;
  
  struct token_iterator* iterator;
};

struct parser_stage2* parser_stage2_new(struct parser_stage1* lexer);
void parser_stage2_free(struct parser_stage2* self);

// 0 on success
// Errors:
// -EFAULT: Error parsing (check self->errormsg)
// -ENOMEM: Not enough memory
// -EINVAL: Attempt to process failed instance
int parser_stage2_process(struct parser_stage2* self, struct bytecode** result);

// 0 on success
// Errors:
// -ENOMEM: Not enough memory
int parser_stage2_get_label(struct parser_stage2_context* ctx, const char* name, struct code_emitter_label** result);

// Prototype temporary ID (which will be resolved to actual ID) or negative on error
// Errors:
// -ENOSPC: Too many prototypes
// -ENOMEM: Not enough memory
int64_t parser_stage2_get_prototype_id(struct parser_stage2_context* ctx, const char* name);

#endif

