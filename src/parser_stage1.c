#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>

#include "bytecode/bytecode.h"
#include "bytecode/prototype.h"
#include "parser_stage1.h"
#include "common.h"
#include "util.h"
#include "vec.h"

struct parser_stage1* parser_stage1_new(struct lexer* lexer) {
  struct parser_stage1* self = malloc(sizeof(*self));
  self->currentLexer = NULL;
  self->canFreeErrorMsg = false;
  self->errormsg = NULL;
  return self;
}

void parser_stage1_free(struct parser_stage1* self) {
  if (self->canFreeErrorMsg)
    free((void*) self->errormsg);
  free(self);
}

ATTRIBUTE_PRINTF(2, 3)
static int setError(struct parser_stage1* self, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  self->canFreeErrorMsg = false;
  self->errormsg = common_format_error_message_valist(self->currentLexer->inputName, 
                                               "parsing error", 
                                               self->currentToken->startLine + 1, 
                                               self->currentToken->startColumn + 1, 
                                               fmt,
                                               args);
  va_end(args);
  
  if (!self->errormsg) {
    self->errormsg = "parser_stage1: Cannot allocate error message";
    return -EINVAL;
  }
  
  self->canFreeErrorMsg = true;
  return 0;
}

int parser_stage1_process(struct parser_stage1* self, struct lexer* lexer) {
  self->currentToken = NULL;
  self->currentLexer = lexer;
  
  return 0;
}
