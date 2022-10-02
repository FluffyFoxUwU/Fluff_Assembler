#ifndef _headers_1664541634_Fluff_Assembler_common
#define _headers_1664541634_Fluff_Assembler_common

#include <stdarg.h>

char* common_format_error_message_valist(const char* filename, const char* source, int line, int column, const char* fmt, va_list args);
char* common_format_error_message(const char* filename, const char* source, int line, int column, const char* fmt, ...);

#endif

