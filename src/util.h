#ifndef header_1664455065_ca91d6b5_5c0f_4230_9aad_b5dd1301e644_util_h
#define header_1664455065_ca91d6b5_5c0f_4230_9aad_b5dd1301e644_util_h

#include <stdarg.h>
#include <stddef.h>

#include "compiler_config.h"

size_t util_vasprintf(char** buffer, const char* fmt, va_list args);

ATTRIBUTE_PRINTF(2, 3)
size_t util_asprintf(char** buffer, const char* fmt, ...);

typedef void (^runnable_block)();

#endif

