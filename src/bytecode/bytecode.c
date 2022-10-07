#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "bytecode.h"
#include "prototype.h"
#include "constants.h"
#include "vm_types.h"
#include "vec.h"

struct bytecode* bytecode_new() {
  struct bytecode* self = malloc(sizeof(*self));
  if (!self)
    return NULL;
  
  self->mainPrototype = NULL;
  vec_init(&self->constants);
  return self;
}

void bytecode_free(struct bytecode* self) {
  if (!self)
    return;

  prototype_free(self->mainPrototype);
  
  vec_deinit(&self->constants);
  free(self);
}




