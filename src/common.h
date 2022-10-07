#ifndef _headers_1664541634_Fluff_Assembler_common
#define _headers_1664541634_Fluff_Assembler_common

#include <stdarg.h>

#include "compiler_config.h"

struct token;

// All line and column number is zero based

char* common_format_error_message_valist(const char* filename, const char* source, int line, int column, const char* fmt, va_list args);
ATTRIBUTE_PRINTF(5, 6)
char* common_format_error_message(const char* filename, const char* source, int line, int column, const char* fmt, ...);

char* common_format_error_message_about_token_valist(const char* filename, const char* source, int line, int column, struct token* token, const char* fmt, va_list args);
ATTRIBUTE_PRINTF(6, 7)
char* common_format_error_message_about_token(const char* filename, const char* source, int line, int column, struct token* token, const char* fmt, ...);

#endif

