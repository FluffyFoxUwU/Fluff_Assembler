#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "util.h"
#include "common.h"
#include "lexer.h"

char* common_format_error_message_valist(const char* filename, const char* source, int line, int column, const char* fmt, va_list args) {
  char* result = NULL;
  char* errmsg = NULL;
  util_vasprintf(&errmsg, fmt, args);
  if (!errmsg)
    return NULL;
  
  util_asprintf(&result, "%s: %s:%d:%d: %s", source, filename, line + 1, column + 1, errmsg);
  free(errmsg);
  return result;
}

char* common_format_error_message(const char* filename, const char* source, int line, int column, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char* tmp = common_format_error_message_valist(filename, source, line, column, fmt, args);
  va_end(args); 
  return tmp;
}

char* common_format_error_message_about_token_valist(const char* filename, const char* source, int line, int column, struct token* token, const char* fmt, va_list args) {
  // Both `line` and `column` must have same signedness 
  // (e.g. there `line` cannot be negative if `column` positive)
  if ((line < 0 || column < 0) && (line >= 0 || column >= 0))
    return NULL;
  
  char* errmsg;
  util_vasprintf(&errmsg, fmt, args);
  if (!errmsg)
    return NULL;
  
  char* hand = NULL;
  size_t len = buffer_length(token->rawToken);
  if ((line < 0 || column < 0) && len > 0) {
    hand = malloc(len);
    if (!hand)
      return NULL;
    
    memset(hand, '~', len - 1);
    hand[len - 1] = '\0';
  }
  
  char* res = common_format_error_message(filename, 
                              source, 
                              line < 0 ? token->startLine : line, 
                              column < 0 ? token->startColumn : column, 
                              "%s\n%s\n%*s^%s",
                              errmsg,
                              buffer_string(token->fullLine),
                              column < 0 ? token->startColumn : column,
                              "",
                              hand ? hand : "");
  
  free(hand);
  free(errmsg);
  return res;
}

char* common_format_error_message_about_token(const char* filename, const char* source, int line, int column, struct token* token, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char* res = common_format_error_message_about_token_valist(filename, source, line, column, token, fmt, args);
  va_end(args);
  return res;
}
