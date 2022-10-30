#ifndef _headers_1667133524_Fluff_Assembler_assembler_driver
#define _headers_1667133524_Fluff_Assembler_assembler_driver

#include <stdio.h>

// Convenience layer for assembler

// Resulting in protobuf encoded bytecode (with the bytecode magic)
// `errorMessage` must be free'd on error
// Return 0 on success
// Errors:
// -EFAULT: Failure assembling
// -ENOMEM: No memory
int assembler_driver_assemble(const char* inputName, FILE* inputFile, const char** errorMessage, void** result, size_t* resultSize);

#endif

