#ifndef _headers_1665895918_Fluff_Assembler_parser_stage2
#define _headers_1665895918_Fluff_Assembler_parser_stage2

#include <stdbool.h>
#include <stddef.h>

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

typedef HASHMAP(char, struct code_emitter_label) parser_stage2_label_name_to_label;

struct parser_stage2 {
  struct parser_stage1* parser;
  struct statement_compiler* statementCompiler;
  
  bool isCompleted;
  bool canFreeErrorMsg;
  bool isFirstStatement;
  const char* errorMessage;
  
  const char* currentInputName;
  
  int statementPointer; 
  struct statement* currentStatement;
  
  struct bytecode* bytecode;
  struct parser_stage2_context* currentCtx;
  
  bool isStatementCompilerRegistered;
};

struct parser_stage2_context {
  struct parser_stage2* owner;
  struct prototype* proto;
  struct code_emitter* emitter;
  parser_stage2_label_name_to_label* labelLookup;
  
  struct token_iterator* iterator;
};

struct parser_stage2* parser_stage2_new(struct parser_stage1* lexer);
void parser_stage2_free(struct parser_stage2* self);

// 0 on success
// Errors:
// -EFAULT: Error parsing (check self->errormsg)
// -ENOMEM: Not enough memory
// -EINVAL: Attempt to process failed instance
int parser_stage2_process(struct parser_stage2* self);

// 0 on success
// Errors:
// -ENOMEM: Not enough memory
int parser_stage2_get_label(struct parser_stage2_context* ctx, const char* name, struct code_emitter_label** result);

#endif

