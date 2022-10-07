#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "prototype.h"
#include "constants.h"
#include "vec.h"
#include "vm_types.h"

struct prototype* prototype_new(const char* sourceFile, const char* prototypeName, int line, int column) {
  struct prototype* self = malloc(sizeof(*self));
  if (!self)
    return NULL;
  
  vec_init(&self->prototypes);
  vec_init(&self->instructions);
  
  self->sourceFile = NULL;
  self->prototypeName = NULL;
  self->definedAtLine = line;
  self->definedAtColumn = column;
  
  self->sourceFile = strdup(sourceFile);
  if (!self->sourceFile)
    goto out_of_mem;
  
  self->prototypeName = strdup(prototypeName);
  if (!self->prototypeName)
    goto out_of_mem;
  return self;
  
out_of_mem:
  prototype_free(self);
  return NULL;
}

void prototype_free(struct prototype* self) {
  if (!self)
    return;
  
  vec_deinit(&self->prototypes);
  vec_deinit(&self->instructions);
  free((char*) self->prototypeName);
  free((char*) self->sourceFile);
  free(self);
}

