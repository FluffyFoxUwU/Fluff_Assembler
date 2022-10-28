#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include "hashmap.h"
#include "parser_stage2.h"
#include "token_iterator.h"
#include "parser_stage1.h"
#include "code_emitter.h"
#include "hashmap.h"
#include "statement_compiler.h"

struct statement_compiler* statement_compiler_new(struct parser_stage2* parser, struct statement_compiler* overrideBy) {
  if (parser && 
     overrideBy && overrideBy->parser && 
     overrideBy->parser != parser) {
    return NULL;
  }
  
  struct statement_compiler* self = malloc(sizeof(*self));
  if (!self)
    return NULL;
  self->overrideBy = overrideBy;
  self->parser = parser;
  hashmap_init(&self->emitterRegistry, hashmap_hash_string, strcmp);
  
  return self;
}

void statement_compiler_free(struct statement_compiler* self) {
  const char* k;
  struct statement_processor* v;
  
  // TODO: Add code to warn about registered processor which not unregistered
  hashmap_foreach(k, v, &self->emitterRegistry)
    statement_compiler_unregister(self, k);
  hashmap_cleanup(&self->emitterRegistry);
  
  if (!self)
    return;
  free(self);
}

int statement_compiler_register(struct statement_compiler* self, const char* name, struct statement_processor* entry) {
  struct statement_processor* newEntry = malloc(sizeof(*newEntry));
  if (!newEntry)
    return -ENOMEM;
  
  *newEntry = *entry;
  newEntry->name = strdup(name);
  newEntry->owner = self;

  int res = hashmap_put(&self->emitterRegistry, name, newEntry);
  if (res < 0)
    free(newEntry);
  return res;
}

int statement_compiler_unregister(struct statement_compiler* self, const char* name) {
  struct statement_processor* entry = hashmap_remove(&self->emitterRegistry, name);
  if (entry == NULL)
    return -EADDRNOTAVAIL;
  
  free((char*) entry->name);
  free(entry);
  return 0;
}

int statement_compile(struct statement_compiler* self, struct parser_stage2_context* context, struct statement* statement, bool* canFreeErr, char** err) {
  if (context->iterator->current == NULL)
    return -EINVAL;
  
  struct statement_processor* funcEntry = hashmap_get(&self->emitterRegistry, context->iterator->current->data.identifier->data);
  if (!funcEntry)
    return -EADDRNOTAVAIL;
  
  struct statement_processor_context ctx = {
    .stage2Context = context,
    .statement = statement,
    .canFreeErr = false,
    .err = NULL,
    .funcEntry = funcEntry,
    .owner = self,
    .instructionToken = context->iterator->current
  };
  int res = funcEntry->processor(&ctx);
  *err = (char*) ctx.err;
  *canFreeErr = ctx.canFreeErr;
  return res;
}
