#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "common.h"
#include "code_emitter.h"
#include "parser_stage2.h"
#include "parser_stage1.h"
#include "lexer.h"
#include "bytecode/prototype.h"
#include "bytecode/bytecode.h"
#include "constants.h"

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
  self->statementPointer = false;
  
  self->bytecode = bytecode_new();
  if (self->bytecode == NULL)
    goto failure;
  
  return self;
failure:
  parser_stage2_free(self);
  return NULL;
}

void parser_stage2_free(struct parser_stage2* self) {
  if (self->canFreeErrorMsg)
    free((char*) self->errorMessage);
  
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
                                               self->currentStatement->rawTokens.data[0], 
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
  struct code_emitter* emitter = NULL;
  struct prototype* proto = prototype_new(filename, prototypeName, line, column);
  if (proto == NULL) {
    res = -ENOMEM;
    goto prototype_alloc_fail;
  }
  
  emitter = code_emitter_new();
  if (!emitter) {
    res = -ENOMEM;
    goto emitter_alloc_fail;
  }

  // Process instructions
  bool isStartSymbol = strcmp(prototypeName, ASSEMBLER_START_SYMBOL);
  while (true) {
    if (isStartSymbol && self->statementPointer >= self->parser->allStatements.length)
      break;
    
    struct statement* current = self->currentStatement;
    if (current->type == STATEMENT_INSTRUCTION) {
    
    } else {
      if ((res = setError(self, "Unexpected statement type!")) == 0)
        res = -EFAULT; 
      goto processing_failed;
    }
    
    if ((res = getNextStatement(self)) < 0)
      goto get_next_statement_failed;
  }

processing_failed:
get_next_statement_failed:
emitter_alloc_fail:
  code_emitter_free(emitter); 
  
  if (res < 0) { 
    prototype_free(proto);
    proto = NULL;
  }
  
  if (result)
    *result = proto;
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
