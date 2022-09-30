#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "prototype.h"
#include "constants.h"
#include "vm_types.h"

struct prototype* prototype_new() {
  struct prototype* self = malloc(sizeof(*self));
  if (!self)
    return NULL;

  self->owner = NULL;
  self->code = NULL;
  self->codeLen = 0;
  return self;
}

int prototype_set_code(struct prototype* self, size_t codeLen, vm_instruction* code) {
  vm_instruction* copied = calloc(codeLen, sizeof(*copied));
  if (!copied)
    return -ENOMEM;
  memcpy(copied, code, sizeof(*copied) * codeLen);
  
  // Free old code
  free(self->code);

  self->codeLen = codeLen;
  self->code = copied;
  return 0;
}

void prototype_free(struct prototype* self) {
  if (!self)
    return;

  free(self->code);
  free(self);
}

