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
#include "token_iterator.h"
#include "hashmap_base.h"
#include "parser_stage2.h"
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
ATTRIBUTE_PRINTF(2, 3)
static int setError(struct parser_stage2* self, const char* fmt, ...) {
  self->canFreeErrorMsg = false;
  
  if (self->currentCtx == NULL) {
    self->errorMessage = "Invalid setError call: self->currentCtx is NULL (this is a bug please report this cant occur in anyway)";
    return -EINVAL;
  } else if (self->errorMessage != NULL) {
    self->errorMessage = "Invalid setError call: self->errorMessage is non NULL (this is a bug please report this cant occur in anyway)";
    return -EINVAL;
  }

  va_list args;
  va_start(args, fmt);
  self->errorMessage = NULL;
  
  if (self->currentCtx->iterator && self->currentCtx->iterator->current) {
    self->errorMessage = common_format_error_message_about_token_valist(self->currentInputName, 
                                                 "parsing stage2 error", 
                                                 -1, -1,
                                                 self->currentCtx->iterator->current, 
                                                 fmt, args);
  } else {
    self->errorMessage = common_format_error_message_valist(self->currentInputName, 
                                                 "parsing stage2 error", 
                                                 -1, -1,
                                                 fmt, args);
  }
  
  va_end(args);
  
  if (!self->errorMessage) {
    self->errorMessage = "parser_stage2: Cannot allocate error message";
    return -ENOMEM;
  }
  
  self->canFreeErrorMsg = true;
  return 0;
}

// Error:
// -ERANGE: No data to read
// -ENOMEM: Not enough memory
static int getNextStatement(struct parser_stage2* self) {
  if (self->nextStatementPointer >= self->parser->allStatements.length) {
    if (setError(self, "No statement to read") < 0)
      return -ENOMEM; 
    return -ERANGE;
  }
  
  self->currentStatement = self->parser->allStatements.data[self->nextStatementPointer];
  self->nextStatementPointer++;
  return 0;
}

static int processInstruction(struct parser_stage2* self, struct parser_stage2_context* ctx) {
  int res = 0;
  char* err = NULL;
  bool canFreeErr = false;
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

error_adding_prototype:
prototype_generation_error:
get_statement_failed:
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

static int processPrototype(struct parser_stage2* self, const char* filename, const char* prototypeName, int line, int column, struct prototype** result) {
  const char* oldInputName = self->currentInputName;
  self->currentInputName = filename;
  
  int res = 0;
  struct parser_stage2_context ctx = {
    .owner = self,
    .emitter = NULL,
    .labelLookup = NULL,
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

  ctx.labelLookup = malloc(sizeof(*ctx.labelLookup));
  if (!ctx.labelLookup) {
    res = -ENOMEM;
    goto label_lookup_alloc_failed;
  }
  
  hashmap_init(ctx.labelLookup, hashmap_hash_string, strcmp);

  // Process instructions
  bool isMainChunk = strcmp(prototypeName, ASSEMBLER_START_SYMBOL) == 0;
  bool isEOFSafe = isMainChunk;
  bool firstIteration = true;
  bool prototypeEnds = false;
  
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
      if (isEOFSafe)
        break;
      
      goto get_next_statement_failed;
    }

prototype_ended:
    token_iterator_free(ctx.iterator);
    ctx.iterator = NULL;
    
    if (prototypeEnds) 
      break;
  }
  
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
  
  // Reserving space
  if (vec_reserve(&ctx.proto->instructions, ctx.emitter->instructions.length) < 0) { 
    res = -ENOMEM;
    goto finalizing_error;
  }

  // This *cannot* fail even if it fail the failure cant be detected and thus be silent
  // corruption!! 
  vec_extend(&ctx.proto->instructions, &ctx.emitter->instructions);

finalizing_error:
get_next_statement_failed:
processing_error:
eof_detected:
  hashmap_cleanup(ctx.labelLookup);
  free(ctx.labelLookup);
failed_alloc_token_iterator:
  token_iterator_free(ctx.iterator);
label_lookup_alloc_failed:
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
    goto failure;
  }
  
  if (getNextStatement(self) < 0) {
    res = -EFAULT;
    goto get_next_statement_error;
  }

  res = processPrototype(self, self->parser->lexer->inputName, ASSEMBLER_START_SYMBOL, 0, 0, &self->bytecode->mainPrototype);

get_next_statement_error:
  if (res < 0) {
    bytecode_free(self->bytecode);
    self->bytecode = NULL;
  }
failure:
  *result = self->bytecode;
  return res;
}

int parser_stage2_get_label(struct parser_stage2_context* ctx, const char* name, struct code_emitter_label** result) {
  struct code_emitter_label* label = hashmap_get(ctx->labelLookup, name);
  if (label)
    goto lookup_hit;
  
  label = code_emitter_label_new(ctx->emitter, ctx->iterator->current);
  if (!label)
    return -ENOMEM;
  
  int err = hashmap_put(ctx->labelLookup, name, label);
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
    *result = label;
  return 0;
}
