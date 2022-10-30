#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "bytecode/bytecode.h"
#include "bytecode/prototype.h"
#include "lexer.h"
#include "parser_stage1.h"
#include "common.h"
#include "util.h"
#include "vec.h"

struct parser_stage1* parser_stage1_new(struct lexer* lexer) {
  struct parser_stage1* self = malloc(sizeof(*self));
  self->lexer = lexer;
  self->canFreeErrorMsg = false;
  self->errorMessage = NULL;
  self->nextTokenPointer = 0;
  self->isFirstToken = true;
  self->currentStatement = NULL;
  self->currentToken = NULL;
  self->isCompleted = false;
  
  vec_init(&self->allStatements);
  return self;
}

static void freeStatement(struct statement* statement) {
  if (!statement)
    return;

  vec_deinit(&statement->rawTokens);
  vec_deinit(&statement->wholeStatement);
  free(statement);
}

void parser_stage1_free(struct parser_stage1* self) {
  if (!self)
    return;
  
  if (self->canFreeErrorMsg)
    free((void*) self->errorMessage);
  
  struct statement* current = NULL;
  int i = 0;
  vec_foreach(&self->allStatements, current, i)
    freeStatement(current);
  
  vec_deinit(&self->allStatements);
  free(self);
}

ATTRIBUTE_PRINTF(2, 3)
static int setError(struct parser_stage1* self, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  self->canFreeErrorMsg = false;
  self->errorMessage = common_format_error_message_about_token_valist(self->lexer->inputName, 
                                               "parsing error", 
                                               -1, -1,
                                               self->currentToken, 
                                               fmt, args);
  va_end(args);
  
  if (!self->errorMessage) {
    self->errorMessage = "parser_stage1: Cannot allocate error message";
    return -ENOMEM;
  }
  
  self->canFreeErrorMsg = true;
  return 0;
}

// Return 0 on success
// Errors:
// -ENAVAIL: No token to read
// -ENOMEM: Out of memory
static int fetchNextTokenRaw(struct parser_stage1* self) {
  if (self->nextTokenPointer >= self->lexer->allTokens.length) {
    setError(self, "No token to read");
    return -ENAVAIL;
  }
  
  self->currentToken = self->lexer->allTokens.data[self->nextTokenPointer];
  self->nextTokenPointer++;
  
  if (vec_push(&self->currentStatement->rawTokens, self->currentToken) < 0)
    return -ENOMEM;
  return 0;
}

static int fetchNextToken(struct parser_stage1* self) {
  do {
    int res = fetchNextTokenRaw(self);
    if (res < 0)
      return res;
  } while (self->currentToken->type == TOKEN_COMMENT);
  
  return 0;
}

// Return 0 on success
// Errors:
// -ENAVAIL: No token to read
// -ENOMEM: Out of memory
static int fetchArgs(struct parser_stage1* self) {
  int res = 0;
  while (self->currentToken->type != TOKEN_COMMA && self->currentToken->type != TOKEN_STATEMENT_END) {
    if (vec_push(&self->currentStatement->wholeStatement, self->currentToken) < 0)
      return -ENOMEM;
    if ((res = fetchNextToken(self)) < 0)
      return res;
    
    if (self->currentToken->type != TOKEN_COMMA)
      break;
    if ((res = fetchNextToken(self)) < 0)
      return res;
  };
  
  return 0;
}

static int processOne(struct parser_stage1* self, struct statement** result) {
  int res = 0;
  self->currentStatement = malloc(sizeof(*self->currentStatement));
  if (!self->currentStatement)
    return -ENOMEM;
  *self->currentStatement = (struct statement) {};
  vec_init(&self->currentStatement->rawTokens);

  // This cant be moved out from this function
  if (self->isFirstToken) {
    self->isFirstToken = false;
    if (fetchNextToken(self) < 0) 
      return -EFAULT;
  }
  
  if (vec_push(&self->currentStatement->rawTokens, self->currentToken) < 0)
    return -ENOMEM;

  bool needEndOfStatament = true;
  switch (self->currentToken->type) {
    case TOKEN_COMMENT:
      needEndOfStatament = false;
      self->currentStatement->type = STATEMENT_COMMENT;
      self->currentStatement->data.commentData = self->currentToken->data.comment;
      if ((res = fetchNextToken(self)) < 0) 
        goto fetch_error; 
      break;
    case TOKEN_LABEL_DECL:
      needEndOfStatament = false;
      self->currentStatement->type = STATEMENT_LABEL_DECLARE;
      self->currentStatement->data.labelName = self->currentToken->data.labelDeclName;
      if (vec_push(&self->currentStatement->wholeStatement, self->currentToken) < 0) {
        res = -ENOMEM;
        goto token_push_error;
      }
      
      if ((res = fetchNextToken(self)) < 0) 
        goto fetch_error;
      break;
    case TOKEN_DIRECTIVE_NAME:
    case TOKEN_IDENTIFIER:
      if (self->currentToken->type == TOKEN_IDENTIFIER)
        self->currentStatement->type = STATEMENT_INSTRUCTION;
      else
        self->currentStatement->type = STATEMENT_ASSEMBLER_DIRECTIVE;
      
      if (vec_push(&self->currentStatement->wholeStatement, self->currentToken) < 0) {
        res = -ENOMEM;
        goto token_push_error;
      }
      fetchNextToken(self);
      if ((res = fetchArgs(self)) < 0)
        goto fetch_error;
      break;
    case TOKEN_STATEMENT_END:
      break;
    default:
      setError(self, "Unexpected token!");
      goto unexpected_token;
  }
  
  if (needEndOfStatament) {
    if (self->currentToken->type != TOKEN_STATEMENT_END) {
      setError(self, "Expecting end of statement!");
      goto unexpected_token;
    }
    fetchNextToken(self);
    vec_pop(&self->currentStatement->rawTokens);
  }

  if (result)
    *result = self->currentStatement;
  return res;

unexpected_token:
  res = -EFAULT;
fetch_error:
token_push_error: 
  freeStatement(self->currentStatement);
   return res;
}

int parser_stage1_process(struct parser_stage1* self) {
  if (self->errorMessage || self->isCompleted)
    return -EINVAL;
  self->isCompleted = true;
  
  struct statement* statement;
  int res = 0;
  
  if (self->lexer->allTokens.length == 0)
    goto early_eof;
  
  while (!self->errorMessage && self->nextTokenPointer <= self->lexer->allTokens.length) {
    if ((res = processOne(self, &statement)) < 0)
      break;
    
    if (statement->rawTokens.length == 0) {
      freeStatement(statement);
      
      setError(self, "BUG: Cannot have statement without token (please report)");
      res = -EFAULT;
      break;
    }
    
    if (vec_push(&self->allStatements, statement) < 0) 
      return -ENOMEM;
  }
  
early_eof:
  return res;
}
