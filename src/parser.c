#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>

#include "bytecode/bytecode.h"
#include "bytecode/prototype.h"
#include "parser.h"
#include "common.h"
#include "util.h"
#include "vec.h"

struct parser* parser_new(struct lexer* lexer) {
  struct parser* self = malloc(sizeof(*self));
  self->currentLexer = NULL;
  self->canFreeErrorMsg = false;
  self->errormsg = NULL;
  self->result = NULL;
  self->currentToken = (struct token) {};
  return self;
}

void parser_free(struct parser* self) {
  if (self->result)
    bytecode_free(self->result);
  if (self->canFreeErrorMsg)
    free((void*) self->errormsg);
  free(self);
}

static int setError(struct parser* self, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char* result; 
  util_vasprintf(&result, fmt, args);
  if (!result) {
    self->canFreeErrorMsg = false;
    self->errormsg = "parser: Cannot allocate error message";
    return -EINVAL;
  }
  va_end(args);
  
  self->canFreeErrorMsg = true;
  self->errormsg = common_format_error_message(self->currentLexer->inputName, 
                                               "lexing error", 
                                               self->currentToken.startLine + 1, 
                                               self->currentToken.startColumn + 1, 
                                               "%s",
                                               result,
                                               "");
  return 0;
}

static int processPrototype(struct parser* self) {
  int res = 0;
  struct prototype* proto = prototype_new();
  if (!proto) {
    res = -ENOMEM;
    goto failure;
  }
  
  vec_t(struct prototype*) prototyes;
  vec_init(&prototyes);
  
  vec_deinit(&prototyes);
  
failure:
  prototype_free(proto);
  return res;
}

int parser_process(struct parser* self, struct lexer* lexer) {
  self->result = bytecode_new();
  if (!self->result)
    return -ENOMEM;
  
  self->currentToken = (struct token) {};
  self->currentLexer = lexer;
  return 0;
}
