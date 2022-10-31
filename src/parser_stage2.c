#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "code_emitter.h"
#include "default_statement_processors.h"
#include "hashmap.h"
#include "opcodes.h"
#include "token_iterator.h"
#include "hashmap_base.h"
#include "parser_stage2.h"
#include "vm_limits.h"
#include "parser_stage1.h"
#include "lexer.h"
#include "bytecode/prototype.h"
#include "bytecode/bytecode.h"
#include "constants.h"
#include "statement_compiler.h"
#include "util.h"
#include "vec.h"
#include "vm_types.h"

struct parser_stage2* parser_stage2_new(struct parser_stage1* parser) {
  struct parser_stage2* self = malloc(sizeof(*self));
  if (!self)
    return NULL;
  
  self->bytecode = NULL;
  self->canFreeErrorMsg = false;
  self->parser = parser;
  self->isCompleted = false;
  self->isFirstStatement = true;
  self->currentStatement = NULL;
  self->errorMessage = NULL;
  self->statementCompiler = NULL;
  self->currentCtx = NULL;
  self->errorMessage = NULL;
  self->isStatementCompilerRegistered = false;
  self->nextStatementPointer = 0;
  self->currentInputName = NULL;
  self->currentStatement = NULL;
  
  self->statementCompiler = statement_compiler_new(self, NULL);
  if (!self->statementCompiler)
    goto failure;
 
  if (default_processor_register(self->statementCompiler, self) < 0)
    goto failure;
  self->isStatementCompilerRegistered = true;
  
  return self;
failure:
  parser_stage2_free(self);
  return NULL;
}

void parser_stage2_free(struct parser_stage2* self) {
  if (!self)
    return;
  
  if (self->canFreeErrorMsg)
    free((char*) self->errorMessage);
  
  if (self->isStatementCompilerRegistered)
    default_processor_unregister(self->statementCompiler, self);
  
  statement_compiler_free(self->statementCompiler);
  free(self);
}

// Erorrs:
// -ENOMEM: Not enough memory
static int vsetErrorWithToken(struct parser_stage2* self, struct token* token, const char* fmt, va_list args) {
  self->canFreeErrorMsg = false;
  
  if (self->currentCtx == NULL) {
    self->errorMessage = "Invalid setError call: self->currentCtx is NULL (this is a bug please report this cant occur in anyway)";
    return -EINVAL;
  } else if (self->errorMessage != NULL) {
    self->errorMessage = "Invalid setError call: self->errorMessage is non NULL (this is a bug please report this cant occur in anyway)";
    return -EINVAL;
  }

  self->errorMessage = NULL;
  
  if (token) {
    self->errorMessage = common_format_error_message_about_token_valist(self->currentInputName, 
                                                 "parsing stage2 error", 
                                                 -1, -1,
                                                 token, 
                                                 fmt, args);
  } else {
    self->errorMessage = common_format_error_message_valist(self->currentInputName, 
                                                 "parsing stage2 error", 
                                                 -1, -1,
                                                 fmt, args);
  }

  if (!self->errorMessage) {
    self->errorMessage = "parser_stage2: Cannot allocate error message";
    return -ENOMEM;
  }
  
  self->canFreeErrorMsg = true;
  return 0;
}

ATTRIBUTE_PRINTF(3, 4)
static int setErrorWithToken(struct parser_stage2* self, struct token* token, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int res = vsetErrorWithToken(self, token, fmt, args);
  va_end(args);
  return res;
}

ATTRIBUTE_PRINTF(2, 3)
static int setError(struct parser_stage2* self, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int res = vsetErrorWithToken(self, self->currentCtx->iterator ? self->currentCtx->iterator->current : NULL, fmt, args);
  va_end(args);
  return res;
}

static int getNextStatementRaw(struct parser_stage2* self) {
  if (self->nextStatementPointer >= self->parser->allStatements.length) 
    return -ERANGE;
  
  self->currentStatement = self->parser->allStatements.data[self->nextStatementPointer];
  self->nextStatementPointer++;
  return 0;
}

// Error:
// -ERANGE: No data to read
// -ENOMEM: Not enough memory
static int getNextStatement(struct parser_stage2* self) {
  int res = getNextStatementRaw(self);
  if (res == -ERANGE && setError(self, "No statement to read") < 0) {
    res = -ENOMEM;
    goto failure;
  }

failure:
  return res;
}

static int processInstruction(struct parser_stage2* self, struct parser_stage2_context* ctx) {
  int res = 0;
  char* err = NULL;
  bool canFreeErr = false;
  
  if (vec_push(&ctx->ipToStamement, self->currentStatement) < 0) {
    res = -ENOMEM;
    goto record_line_info_failed;
  }
  
  int statementCompilerRes = statement_compile(self->statementCompiler, ctx, self->currentStatement, &canFreeErr, &err);
  
  // TODO: Check for junk tokens and report it at this line
  
  switch (statementCompilerRes) {
    case -ENOMEM:
      setError(self, "statement_compiler: Not enough memory");
      res = -EFAULT;
      goto processing_error;
    case -EINVAL:
      setError(self, "statement_compiler: Invalid call");
      res = -EFAULT;
      goto processing_error;
    case -EFAULT:
      setError(self, "statement_compiler: %s", err);
      if (canFreeErr)
        free(err);
      res = -EFAULT;
      goto processing_error;
    case -ENOSPC:
      setError(self, "statement_compiler: Too many instructions generated");
      res = -EFAULT;
      goto processing_error;
    case -EADDRNOTAVAIL:
      setError(self, "Unknown instruction '%s'", ctx->iterator->current->data.identifier->data);
      res = -EFAULT;
      goto processing_error;
    default:
      if (statementCompilerRes >= 0)
        break;
      setError(self, "Unknown statement compiler error %d! (This a bug please report)", statementCompilerRes);
      res = -EFAULT;
      goto processing_error;
  }

processing_error:
record_line_info_failed:
  return res;
}

static int processLabelDecl(struct parser_stage2* self, struct parser_stage2_context* ctx) {
  int res = 0;
  const char* labelName = buffer_string(ctx->iterator->current->data.labelDeclName);
  struct code_emitter_label* label;
  
  if ((res = parser_stage2_get_label(ctx, labelName, &label)) < 0)
    goto label_lookup_failed;
  
  if (code_emitter_label_define(ctx->emitter, label) < 0) {
    setError(self, "Double label definitions: Label \'%s\'", labelName);
    res = -EFAULT;
    goto double_label_define;
  }

double_label_define:
label_lookup_failed:
  return res;
}

static int processPrototype(struct parser_stage2* self, const char* filename, const char* prototypeName, int line, int column, struct prototype** result);

static int processStartPrototypeDirective(struct parser_stage2* self, struct parser_stage2_context* ctx) {
  struct prototype* newPrototype = NULL;
  const char* filename = self->currentInputName;
  const char* prototypeName = NULL;
  int line = ctx->iterator->current->startLine;
  int column = ctx->iterator->current->startColumn;
  
  int res = token_iterator_next_identifier(ctx->iterator, &prototypeName);
  if (res == -EINVAL)
    setError(self, "Identifier expected");
  else if (res == -ENODATA)
    setError(self, "Expecting name");
  
  int64_t res2 = parser_stage2_get_prototype_id(ctx, prototypeName);
  if (res2 < 0) {
    res = (int) res2;
    goto get_prototype_id_failed;
  }
  
  struct prototype_registry_entry* entry = ctx->prototypesRegistryEntries.data[res2];
  if (entry->proto != NULL) {
    setError(self, "Double prototype definition!");
    res = -EFAULT;
    goto duplicate_prototype;
  }
  
  if ((res = getNextStatement(self)) < 0)
    goto get_statement_failed;
  
  res = processPrototype(self, filename, prototypeName, line, column, &newPrototype);
  if (res < 0)
    goto prototype_generation_error;
  
  if (vec_push(&ctx->proto->prototypes, newPrototype) < 0) {
    prototype_free(newPrototype);
    res = -ENOMEM;
    goto error_adding_prototype;
  }

  entry->resolvedLocation = ctx->proto->prototypes.length - 1;
  entry->proto = newPrototype;
error_adding_prototype:
prototype_generation_error:
get_statement_failed:
duplicate_prototype:
get_prototype_id_failed:
  return res;
}

// Return 0 on success or 1 if current prototype ends
// Errors:
// -EFAULT: Error
static int processAssemblerDirective(struct parser_stage2* self, struct parser_stage2_context* ctx) {
  const char* directive = buffer_string(ctx->iterator->current->data.directiveName);
  int res = 0;
  if (strcmp(directive, "start_prototype") == 0) {
    res = processStartPrototypeDirective(self, ctx);
  } else if (strcmp(directive, "end_prototype") == 0) {
    res = 1;
  } else {
    setError(self, "Unknown directive");
    res = -EFAULT;
    goto invalid_directive;
  }
  
  invalid_directive:
  return res;
}

static int fixPrototypeLoads(struct parser_stage2_context* ctx) {
  int res = 0;
  int i = 0;
  vm_instruction* current = NULL;
  
  vec_foreach_ptr(&ctx->emitter->instructions, current, i) {
    if ((*current & 0xFF00'0000'0000'0000l) >> 56 != FLUFFYVM_OPCODE_IMPLDEP1)
      continue;
    
    uint32_t temporaryID = (*current & 0x0000'0000'FFFF'FFFFl);
    struct prototype_registry_entry* entry = ctx->prototypesRegistryEntries.data[temporaryID];
    
    if (entry->proto == NULL) {
      struct token* referenceBy = ctx->ipToStamement.data[i]->wholeStatement.data[0];
      setErrorWithToken(ctx->owner, referenceBy, "Undefined prototype '%s' referenced", entry->name);
      res = -EFAULT;
      goto unknown_prototype_load;
    }
    
    // Patch up the instruction 
    *current = OP_ARG_OPCODE(FLUFFYVM_OPCODE_LOAD_PROTOTYPE) |
                         OP_ARG_COND((*current & 0x00FF'0000'0000'0000l) >> 48) |
                         OP_ARG_A_U16_U32((*current & 0x0000'FFFF'0000'0000l) >> 32) |
                         OP_ARG_B_U16_U32(entry->resolvedLocation);
  }
  
unknown_prototype_load:
  return res;
}

static int processPrototype(struct parser_stage2* self, const char* filename, const char* prototypeName, int line, int column, struct prototype** result) {
  const char* oldInputName = self->currentInputName;
  self->currentInputName = filename;
  
  int res = 0;
  struct parser_stage2_context ctx = {
    .owner = self,
    .emitter = NULL,
    .proto = NULL,
    .iterator = NULL
  };
  struct parser_stage2_context* oldCtx = self->currentCtx;
  self->currentCtx = &ctx;
  
  ctx.proto = prototype_new(filename, prototypeName, line, column);
  if (ctx.proto == NULL) {
    res = -ENOMEM;
    goto prototype_alloc_fail;
  }
  
  ctx.emitter = code_emitter_new();
  if (!ctx.emitter) {
    res = -ENOMEM;
    goto emitter_alloc_fail;
  }

  hashmap_init(&ctx.labelLookup, hashmap_hash_string, strcmp);
  hashmap_init(&ctx.prototypesRegistry, hashmap_hash_string, strcmp);
  vec_init(&ctx.prototypesRegistryEntries);
  vec_init(&ctx.ipToStamement);

  // Process instructions
  bool isMainChunk = strcmp(prototypeName, ASSEMBLER_START_SYMBOL) == 0;
  bool isEOFSafe = isMainChunk;
  bool firstIteration = true;
  bool prototypeEnds = false;
  
  if (self->parser->allStatements.length == 0)
    goto early_eof;
  
  while (true) {
    if (self->nextStatementPointer >= self->parser->allStatements.length && !firstIteration) {
      if (!isEOFSafe) {
        setError(self, "EOF detected!");
        res = -EFAULT;
        goto eof_detected;
      }
      
      // Can exit earlier if this is main chunk or prototype ended
      break;
    }
    firstIteration = true;
    
    struct statement* current = self->currentStatement;
    ctx.iterator = token_iterator_new(current);
    token_iterator_next(ctx.iterator, NULL);
    
    if (!ctx.iterator) {
      res = -ENOMEM;
      goto failed_alloc_token_iterator;
    }
    
    switch (current->type) {
      case STATEMENT_INSTRUCTION:
        if ((res = processInstruction(self, &ctx)) < 0)
          goto processing_error;
        break;
      case STATEMENT_LABEL_DECLARE:
        if ((res = processLabelDecl(self, &ctx)) < 0)
          goto processing_error;
        break;
      case STATEMENT_ASSEMBLER_DIRECTIVE:
        res = processAssemblerDirective(self, &ctx);
        if (res < 0) {
          goto processing_error;
        } else if (res == 1) { 
          // Other code expect 0 on success
          res = 0;
          prototypeEnds = true;
          goto prototype_ended;
        }
        break;
      default:
        setError(self, "Unexpected statement type!");
        res = -EFAULT; 
        goto processing_error;
    }
    
    if ((res = getNextStatement(self)) < 0) {
      // If eof safe other code didnt expect error message is set
      if (isEOFSafe) {
        if (self->canFreeErrorMsg)
          free((char*) self->errorMessage);
        
        self->errorMessage = NULL;
        self->canFreeErrorMsg = false;
        break;
      }
      
      goto get_next_statement_failed;
    }

prototype_ended:
    token_iterator_free(ctx.iterator);
    ctx.iterator = NULL;
    
    if (prototypeEnds) 
      break;
  }

early_eof:
  res = code_emitter_finalize(ctx.emitter);
  if (res == -EFAULT) {
    char* tmp;
    util_asprintf(&tmp, "Code emitter error: %s", ctx.emitter->errorMessage);
    self->errorMessage = tmp;
    if (!tmp) {
      res = -ENOMEM;
      goto finalizing_error;
    }
    self->canFreeErrorMsg = true;
  }
  
  if ((res = fixPrototypeLoads(&ctx)) < 0)
    goto fix_prototype_load_failure;

  // Reserving space
  if (vec_reserve(&ctx.proto->instructions, ctx.emitter->instructions.length) < 0) { 
    res = -ENOMEM;
    goto finalizing_error;
  }

  // This *cannot* fail even if it fail the failure cant be detected and thus be silent
  // corruption!! 
  vec_extend(&ctx.proto->instructions, &ctx.emitter->instructions);

finalizing_error:
fix_prototype_load_failure:
get_next_statement_failed:
processing_error:
eof_detected:
  hashmap_cleanup(&ctx.labelLookup);
  hashmap_cleanup(&ctx.prototypesRegistry);
  
  int i = 0;
  struct prototype_registry_entry* current = NULL;
  vec_foreach(&ctx.prototypesRegistryEntries, current, i) { 
    free((char*) current->name);
    free(current);
  }
  
  vec_deinit(&ctx.prototypesRegistryEntries);
  vec_deinit(&ctx.ipToStamement);
failed_alloc_token_iterator:
  token_iterator_free(ctx.iterator);
  code_emitter_free(ctx.emitter); 
emitter_alloc_fail:
  // Only free prototype on failure
  if (res < 0) { 
    prototype_free(ctx.proto);
    ctx.proto = NULL;
  }
  
  if (result)
    *result = ctx.proto;
prototype_alloc_fail:
  self->currentInputName = oldInputName;
  self->currentCtx = oldCtx;
  return res;
}

int parser_stage2_process(struct parser_stage2* self, struct bytecode** result) {
  if (self->isCompleted)
    return -EINVAL;
  self->isCompleted = true;
  
  int res = 0;
  self->bytecode = bytecode_new();
  if (self->bytecode == NULL) {
    res = -ENOMEM;
    goto bytecode_alloc_failure;
  }
  
  if (getNextStatementRaw(self) == -EFAULT) {
    res = -EFAULT;
    goto get_next_statement_error;
  }

  res = processPrototype(self, self->parser->lexer->inputName, ASSEMBLER_START_SYMBOL, 0, 0, &self->bytecode->mainPrototype);

get_next_statement_error:
  if (res < 0) {
    bytecode_free(self->bytecode);
    self->bytecode = NULL;
  }
bytecode_alloc_failure:
  *result = self->bytecode;
  return res;
}

int parser_stage2_get_label(struct parser_stage2_context* ctx, const char* name, struct code_emitter_label** result) {
  struct code_emitter_label* entry = hashmap_get(&ctx->labelLookup, name);
  if (entry)
    goto lookup_hit;
  
  entry = code_emitter_label_new(ctx->emitter, ctx->iterator->current);
  if (!entry)
    return -ENOMEM;
  
  int err = hashmap_put(&ctx->labelLookup, name, entry);
  switch (err) {
    case -EEXIST:
      setError(ctx->owner, "Error adding label: Label \'%s\' (Unexpected duplicate entry please check concurrent use error)", name);
      break;
    case -ENOMEM:
      setError(ctx->owner, "Error adding label: Label \'%s\' (Out of memory)", name);
      break;
    default:
      if (err < 0)
        setError(ctx->owner, "Unknown error adding label (its a bug please report): Label \'%s\' (Error: %s (%d))", name, strerror(err), err);
      break;
  }

  if (err < 0)
    return -EFAULT;
  
lookup_hit:
  if (result)
    *result = entry;
  return 0;
}

int64_t parser_stage2_get_prototype_id(struct parser_stage2_context* ctx, const char* name) {
  if (ctx->prototypesRegistryEntries.length >= VM_LIMIT_MAX_PROTOTYPE)
    return -ENOSPC;
  
  int64_t res = 0;
  struct prototype_registry_entry* entry = hashmap_get(&ctx->prototypesRegistry, name);
  if (entry) 
    goto lookup_hit;
  
  entry = malloc(sizeof(*entry));
  if (!entry) {
    res = -ENOMEM;
    goto entry_alloc_failed;
  }
  entry->proto = NULL;
  entry->name = strdup(name);
  if (entry->name == NULL) {
    free(entry);
    goto entry_alloc_failed;
  }
  
  if (vec_push(&ctx->prototypesRegistryEntries, entry) < 0) {
    res = -ENOMEM;
    goto insert_failed;
  }
  
  if (hashmap_put(&ctx->prototypesRegistry, name, entry) < 0) {
    vec_pop(&ctx->prototypesRegistryEntries);
    res = -ENOMEM;
    goto insert_failed;
  }
  
  entry->id = ctx->prototypesRegistryEntries.length - 1; 
insert_failed:
  if (res < 0) {
    free((char*) entry->name);
    free(entry);
  }
entry_alloc_failed:
lookup_hit:
  if (res == 0)
    res = entry->id;
  return res;
}
