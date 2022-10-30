#ifndef _headers_1667019986_Fluff_Assembler_vm_limits
#define _headers_1667019986_Fluff_Assembler_vm_limits

#include <stdint.h>

// This limit is per prototype (24 GiB of instructions 
// (this value multiply with fixed instruction size which is 8 bytes))
#define VM_LIMIT_MAX_CODE (UINT32_C(1) << 31)

// This limit is per bytecode
#define VM_LIMIT_MAX_CONSTANT (UINT32_C(1) << 31)

#endif

