#include <protobuf-c/protobuf-c.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/prototype.h"
#include "protobuf_serializer.h"
#include "format/bytecode.pb-c.h"
#include "constants.h"
#include "vec.h"
#include "util.h"
#include "vm_types.h"

// Sanity checks (to catch mistakes of changing sizes without changing the proto file)
static_assert(sizeof(vm_instruction) == FIELD_SIZEOF(FluffyVmFormat__Bytecode__Prototype, instructions[0]));
static_assert(sizeof(vm_int) == FIELD_SIZEOF(FluffyVmFormat__Bytecode__Constant, data_integer));
static_assert(sizeof(vm_number) == FIELD_SIZEOF(FluffyVmFormat__Bytecode__Constant, data_number));

static void freeConstant(FluffyVmFormat__Bytecode__Constant* constant) {
  free(constant);
}

static void freePrototype(FluffyVmFormat__Bytecode__Prototype* prototype) {
  if (!prototype)
    return;
  
  for (int i = 0; i < prototype->n_prototypes; i++)
    freePrototype(prototype->prototypes[i]);
  
  free(prototype->instructions);
  free(prototype->prototypes);
  free(prototype);
}

static void freeBytecode(FluffyVmFormat__Bytecode__Bytecode* bytecode) {
  if (!bytecode)
    return;
  
  freePrototype(bytecode->mainprototype);
  for (int i = 0; i < bytecode->n_constants; i++) 
    freeConstant(bytecode->constants[i]);
  free(bytecode->constants);
  free(bytecode);
}

static FluffyVmFormat__Bytecode__Constant__DataCase convertConstantType(enum constant_type type) {
  static FluffyVmFormat__Bytecode__Constant__DataCase lookup[] = {
    [BYTECODE_CONSTANT_INTEGER] = FLUFFY_VM_FORMAT__BYTECODE__CONSTANT__DATA_DATA_INTEGER,
    [BYTECODE_CONSTANT_STRING] = FLUFFY_VM_FORMAT__BYTECODE__CONSTANT__DATA_DATA_STR,
    [BYTECODE_CONSTANT_NUMBER] = FLUFFY_VM_FORMAT__BYTECODE__CONSTANT__DATA_DATA_NUMBER
  };
  
  if (type < 0 || type > ARRAY_SIZE(lookup))
    return -ERANGE;
  
  return lookup[type];
}

static int convertConstant(FluffyVmFormat__Bytecode__Constant** resultRet, struct constant* constant) {
  FluffyVmFormat__Bytecode__Constant* result = malloc(sizeof(*result));
  if (!result)
    return -ENOMEM;
  
  *result = (FluffyVmFormat__Bytecode__Constant) FLUFFY_VM_FORMAT__BYTECODE__CONSTANT__INIT;
  result->data_case = convertConstantType(constant->type);
  
  switch (constant->type) {
    case BYTECODE_CONSTANT_INTEGER:
      result->data_integer = constant->data.integer;
      break;
    case BYTECODE_CONSTANT_STRING:
      result->data_str.data = (uint8_t*) constant->data.string;
      result->data_str.len = strlen(constant->data.string);
      break;
    case BYTECODE_CONSTANT_NUMBER:
      result->data_number = constant->data.number;
      break;
  }
  
  *resultRet = result;
  return 0;
}

static int convertPrototype(struct prototype* prototype, FluffyVmFormat__Bytecode__Prototype** resultRet) {
  int res = 0;
  FluffyVmFormat__Bytecode__Prototype* result = malloc(sizeof(*result));
  if (result == NULL)
    return -ENOMEM;
  
  *result = (FluffyVmFormat__Bytecode__Prototype) FLUFFY_VM_FORMAT__BYTECODE__PROTOTYPE__INIT;
  result->symbolname = (char*) prototype->prototypeName;
  result->n_prototypes = prototype->prototypes.length;
  result->prototypes = calloc(prototype->prototypes.length, sizeof(void*));
  if (result->prototypes == NULL) {
    res = -ENOMEM;
    goto failure;
  }
  
  int i = 0;
  struct prototype* current = NULL;
  vec_foreach(&prototype->prototypes, current, i)
    if (convertPrototype(current, &result->prototypes[i]) < 0)
      goto failure;
  
  result->n_instructions = prototype->instructions.length;
  result->instructions = calloc(prototype->instructions.length, sizeof(vm_instruction));
  if (result->instructions == NULL) {
    res = -ENOMEM;
    goto failure;
  }
  memcpy(result->instructions, prototype->instructions.data, prototype->instructions.length * sizeof(vm_instruction));

failure:
  if (res < 0) {
    freePrototype(result);
    result = NULL;
  }
  *resultRet = result;
  return res;
}

static int convertBytecode(struct bytecode* bytecode, FluffyVmFormat__Bytecode__Bytecode** resultRet) {
  int res = 0;
  FluffyVmFormat__Bytecode__Bytecode* result = malloc(sizeof(*result));
  if (!result)
    return -ENOMEM;
  *result = (FluffyVmFormat__Bytecode__Bytecode) FLUFFY_VM_FORMAT__BYTECODE__BYTECODE__INIT;
  
  result->version = VM_BYTECODE_VERSION;
  result->n_constants = bytecode->constants.length;
  result->constants = calloc(bytecode->constants.length, sizeof(void*));
  
  if (result->constants == NULL)
    return -ENOMEM;
  
  int i = 0;
  struct constant* constant = NULL;
  vec_foreach_ptr(&bytecode->constants, constant, i)
    if ((res = convertConstant(&result->constants[i], constant)) < 0)
      goto failure;
  
  if (convertPrototype(bytecode->mainPrototype, &result->mainprototype) < 0)
    goto failure;

failure:
  if (res < 0) {
    freeBytecode(result);
    result = NULL;
  }
  *resultRet = result;
  return res;
}

int bytecode_serializer_protobuf(struct bytecode* bytecode, void** result, size_t* size) {
  FluffyVmFormat__Bytecode__Bytecode* protobufBytecode = NULL;
  int res = 0;
  void* buffer = NULL;
  size_t serializedSize = 0;
  
  if ((res = convertBytecode(bytecode, &protobufBytecode)) < 0)
    goto conversion_error;
  
  serializedSize = fluffy_vm_format__bytecode__bytecode__get_packed_size(protobufBytecode);
  if (serializedSize == 0) {
    res = -EINVAL;
    goto size_calculate_error;
  }
  
  buffer = malloc(serializedSize);
  if (!buffer) {
    res = -ENOMEM;
    goto alloc_buffer_failure;
  }
  
  if (fluffy_vm_format__bytecode__bytecode__pack(protobufBytecode, buffer) != serializedSize) {
    buffer = NULL;
    res = -EFAULT;
    goto serialize_failure;
  }

serialize_failure:
  if (res < 0) {
    free(buffer);
    buffer = NULL;
  }

  freeBytecode(protobufBytecode);
alloc_buffer_failure:
size_calculate_error:
conversion_error:
  *result = buffer;
  *size = serializedSize;
  return res;
}
