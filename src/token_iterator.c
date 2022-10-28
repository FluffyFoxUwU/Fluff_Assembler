#include <stdlib.h>
#include <errno.h>

#include "token_iterator.h"
#include "parser_stage1.h"
#include "lexer.h"

struct token_iterator* token_iterator_new(struct statement* statement) {
  struct token_iterator* self = malloc(sizeof(*self));
  if (!self)
    return NULL;
  
  self->pointer = 0;
  self->numTokens = statement->wholeStatement.length;
  self->statement = statement;
  self->tokens = statement->wholeStatement.data;
  self->current = NULL;
  return self;
}

void token_iterator_free(struct token_iterator* self) {
  free(self);
}

int token_iterator_next_identifier(struct token_iterator* self, const char** ident) {
  struct token* tmp = NULL;
  int res = token_iterator_next(self, &tmp);
  if (res < 0)
    goto next_failed;
  if (tmp->type != TOKEN_IDENTIFIER)
    return -EINVAL;
  
next_failed:
  return res;
}

int token_iterator_next(struct token_iterator* self, struct token** result) {
  if (self->pointer >= self->numTokens)
    return -ENODATA;

  self->current = self->tokens[self->pointer];
  self->pointer++;
  
  if (result)
    *result = self->current;
  
  return 0;
}
