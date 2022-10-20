#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "common.h"
#include "code_emitter.h"
#include "hashmap.h"
#include "hashmap_base.h"
#include "parser_stage2.h"
#include "parser_stage1.h"
#include "lexer.h"
#include "bytecode/prototype.h"
#include "bytecode/bytecode.h"
#include "constants.h"
#include "statement_compiler.h"
#include "util.h"

static int a() {
  puts("ldr instruction");
  return 0;
}

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
  self->statementPointer = false;
  
  self->bytecode = bytecode_new();
  if (self->bytecode == NULL)
    goto failure;
  
  self->statementCompiler = statement_compiler_new(self, NULL);
  if (!self->statementCompiler)
    goto failure;
  statement_compiler_register(self->statementCompiler, "ldr", (struct emitter_func_entry) {
    .type = EMITTER_NO_ARG,
    .func.noArg = (void*) a
  });
  
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
  
  statement_compiler_free(self->statementCompiler);
  bytecode_free(self->bytecode);
  free(self);
}

// Erorrs:
// -ENOMEM: Not enough memory
ATTRIBUTE_PRINTF(2, 3)
static int setError(struct parser_stage2* self, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  self->canFreeErrorMsg = false;
  self->errorMessage = common_format_error_message_about_token_valist(self->currentInputName, 
                                               "parsing stage2 error", 
                                               -1, -1,
                                               self->currentStatement->wholeStatement.data[0], 
                                               fmt, args);
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
  if (self->statementPointer >= self->parser->allStatements.length) {
    if (setError(self, "No statement to read") < 0)
      return -ENOMEM; 
    return -ERANGE;
  }
  
  self->currentStatement = self->parser->allStatements.data[self->statementPointer];
  self->statementPointer++;
  return 0;
}

static int processPrototype(struct parser_stage2* self, const char* filename, const char* prototypeName, int line, int column, struct prototype** result) {
  const char* oldInputName = self->currentInputName;
  self->currentInputName = filename;
    
  int res = 0;
  struct parser_stage2_context ctx = {
    .owner = self,
    .emitter = NULL,
    .labelLookup = NULL,
    .proto = NULL
  };
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
  bool isStartSymbol = strcmp(prototypeName, ASSEMBLER_START_SYMBOL) == 0;
  while (true) {
    if (isStartSymbol && self->statementPointer >= self->parser->allStatements.length)
      break;
    
    struct statement* current = self->currentStatement;
    switch (current->type) {
      case STATEMENT_INSTRUCTION: {
        int statementCompilerRes = statement_compile(self->statementCompiler, &ctx, 0x00, self->currentStatement);
        if (statementCompilerRes < 0) {
          switch (statementCompilerRes) {
            case -ENOMEM:
              setError(self, "statement_compiler: Not enough memory");
              res = -EFAULT;
              goto processing_error;
            case -EFAULT:
              setError(self, "statement_compiler: Code emitter error!");
              res = -EFAULT;
              goto processing_error;
            case -EINVAL:
              setError(self, "statement_compiler: Invalid state!");
              res = -EFAULT;
              goto processing_error;
            case -EADDRNOTAVAIL:
              setError(self, "Unknown instruction '%s'", self->currentStatement->wholeStatement.data[0]->data.identifier->data);
              res = -EFAULT;
              goto processing_error;
            default:
              setError(self, "Unknown statement compiler error %d! (This a bug please report)", statementCompilerRes);
              res = -EFAULT;
              goto processing_error;
          }
        }
        break;
      }
      case STATEMENT_LABEL_DECLARE: {
        struct token* labelToken = current->wholeStatement.data[0];
        const char* labelName = buffer_string(labelToken->data.labelDeclName);
        struct code_emitter_label* label = hashmap_get(ctx.labelLookup, labelName);
        if (!label)
          label = code_emitter_label_new(ctx.emitter, labelToken);
        
        if (!label) {
          res = -ENOMEM;
          goto label_alloc_failed;
        }
        
        if (code_emitter_label_define(ctx.emitter, label) < 0) {
          setError(self, "Double label definitions: Label \'%s\'", labelName);
          res = -EFAULT;
          goto processing_error;
        }

        int err;        
        if ((err = hashmap_put(ctx.labelLookup, labelName, label)) < 0) {
          switch (err) {
            case -EEXIST:
              setError(self, "Error adding label: Label \'%s\' (Unexpected duplicate entry please check concurrent use error)", labelName);
              break;
            case -ENOMEM:
              setError(self, "Error adding label: Label \'%s\' (Out of memory)", labelName);
              break;
            default:
              setError(self, "Unknown error adding label (its a bug please report): Label \'%s\' (Error: %s (%d))", labelName, strerror(err), err);
              break;
          }
          res = -EFAULT;
          goto add_label_error;
        }
        break;
      }
      default:
        setError(self, "Unexpected statement type!");
        res = -EFAULT; 
        goto processing_error;
    }
    
    if ((res = getNextStatement(self)) < 0)
      goto get_next_statement_failed;
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

finalizing_error:
processing_error:
add_label_error:
label_alloc_failed:
get_next_statement_failed:
  hashmap_cleanup(ctx.labelLookup);
  free(ctx.labelLookup);
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
  return res;
}

int parser_stage2_process(struct parser_stage2* self) {
  if (self->isCompleted)
    return -EINVAL;
  self->isCompleted = true;
  
  if (getNextStatement(self) < 0)
    return -EFAULT;

  int res = processPrototype(self, self->parser->lexer->inputName, ASSEMBLER_START_SYMBOL, 0, 0, &self->bytecode->mainPrototype);;
  if (res < 0)
    self->bytecode->mainPrototype = NULL;
  return res;
}
