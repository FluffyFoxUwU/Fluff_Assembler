#ifndef header_1664455065_ca91d6b5_5c0f_4230_9aad_b5dd1301e644_util_h
#define header_1664455065_ca91d6b5_5c0f_4230_9aad_b5dd1301e644_util_h

#include <stdarg.h>
#include <stddef.h>

size_t util_vasprintf(char** buffer, const char* fmt, va_list args);
size_t util_asprintf(char** buffer, const char* fmt, ...);

#endif

