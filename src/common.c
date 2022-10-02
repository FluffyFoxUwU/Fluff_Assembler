#include "util.h"
#include "common.h"
#include <stdarg.h>

char* common_format_error_message_valist(const char* filename, const char* source, int line, int column, const char* fmt, va_list args) {
  char* result = NULL;
  char* errmsg = NULL;
  util_vasprintf(&errmsg, fmt, args);
  if (!errmsg)
    return NULL;
  
  util_asprintf(&result, "%s: %s:%d:%d: %s\n", source, filename, line, column, errmsg);
  return result;
}

char* common_format_error_message(const char* filename, const char* source, int line, int column, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char* tmp = common_format_error_message_valist(filename, source, line, column, fmt, args);
  va_end(args); 
  return tmp;
}
