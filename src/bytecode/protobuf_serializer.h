#ifndef _headers_1667052779_Fluff_Assembler_protobuf_serializer
#define _headers_1667052779_Fluff_Assembler_protobuf_serializer

#include <stddef.h>

// Serialize bytecode with protobuf
struct bytecode;

// `result` and `size` assumed to be non NULL as it doesnt make sense 
// if these were NULL
// Return zero on success
// Errors:
// -ENOMEM: Not enough memory
// -EFAULT: Failure serializing
int bytecode_serializer_protobuf(struct bytecode* bytecode, void** result, size_t* size);

#endif

