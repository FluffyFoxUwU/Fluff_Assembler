#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <errno.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <ctype.h>

#include "buffer.h"
#include "lexer.h"
#include "constants.h"
#include "util.h"
#include "common.h"

struct lexer* lexer_new(FILE* input, const char* inputName) {
  struct lexer* self = malloc(sizeof(*self));
  if (!self)
    return NULL;

  self->input = input;
  self->errorMessage = NULL;
  self->canFreeErrorMessage = false;
  self->lookAhead = '\0';
  self->currentToken = (struct token) {};
  self->currentLine = 0;
  self->currentColumn = -1;
  self->isFirstToken = true;
  self->isThrowingError = false;
  self->inputName = inputName;
  self->currentLineBuffer = NULL;
  self->currentTokenBuffer = NULL;

  self->currentLineBuffer = buffer_new();
  if (!self->currentLineBuffer)
    goto failure;
  
  self->currentTokenBuffer = buffer_new();
  if (!self->currentTokenBuffer)
    goto failure;
  
  return self;

failure:
  lexer_free(self);
  return NULL;
}


void lexer_free(struct lexer* self) {
  if (self->canFreeErrorMessage)
    free((char*) self->errorMessage);
  free(self->currentTokenBuffer);
  free(self->currentLineBuffer);
  free(self);
}

void lexer_free_token(struct token* token) {
  switch (token->type) {
    case TOKEN_STRING:
      free((char*) token->data.string);
      break;
    case TOKEN_LABEL_DECL:
      free((char*) token->data.labelDeclName);
      break;
    case TOKEN_LABEL_REF:
      free((char*) token->data.labelName);
      break;
    case TOKEN_DIRECTIVE_NAME:
      free((char*) token->data.directiveName);
      break;
    case TOKEN_IDENTIFIER:
      free((char*) token->data.identifier);
      break;
    case TOKEN_COMMENT:
      free((char*) token->data.comment);
      break;
    case TOKEN_REGISTER:
    case TOKEN_COMMA:
    case TOKEN_IMMEDIATE:
    case TOKEN_UNKNOWN:
      break;
  }

  free((char*) token->filename);
  free((char*) token->rawToken);
}

static int getCharRaw(struct lexer* self);
static char* formatErrorMessage(struct lexer* self, const char* errmsg) {
  int errorColumn = self->currentColumn + 1;
  int errorLine = self->currentLine + 1;

  while (self->lookAhead != '\n')
    if (getCharRaw(self) < 0)
      break;
  
  return common_format_error_message(self->inputName, 
                                     "lexing error", 
                                     errorLine, 
                                     errorColumn, 
                                     "%s\n%s\n%*s^",
                                     errmsg,
                                     self->currentLineBuffer,
                                     errorColumn - 1,
                                     "");
}

static void throwError_vprintf(struct lexer* self, const char* fmt, va_list args) {
  char* buffer;
  util_vasprintf(&buffer, fmt, args);
  if (!buffer)
    goto format_error;

  self->isThrowingError = true;
  self->canFreeErrorMessage = true;
  self->errorMessage = formatErrorMessage(self, buffer);
  self->isThrowingError = false;
  free(buffer);

  longjmp(self->onError, 1);
  abort();
  
  format_error:
  self->canFreeErrorMessage = false;
  self->errorMessage = "Cannot format error message";

  longjmp(self->onError, 1);
  abort();
}

static void throwError(struct lexer* lexer, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  throwError_vprintf(lexer, fmt, args);
  va_end(args);
}

static void append(struct lexer* self, char c) {
  if (buffer_append_n(self->currentTokenBuffer, &c, 1) < 0)
    throwError(self, "Error appending token buffer");
}

// Return zero on success
// Errors:
// -ENAVAIL: No data left
static int getCharRaw(struct lexer* self) {
  bool isNewLine = self->lookAhead == '\n';
  if (fread(&self->lookAhead, 1, 1, self->input) != 1) {
    if (feof(self->input) != 0)
      return -ENAVAIL;

    if (self->isThrowingError)
      return -EFAULT;

    throwError(self, "Cannot read input (unknown error)");
  }

  if (self->lookAhead != '\n') 
    if (buffer_append_n(self->currentLineBuffer, &self->lookAhead, 1) < 0) 
      throwError(self, "Error appending line buffer");
 
  self->prevColumn = self->currentColumn;
  self->prevLine = self->currentLine;

  self->currentColumn++;
  
  if (isNewLine) {
    self->currentLine++;
    self->currentColumn = 0;

    buffer_clear(self->currentLineBuffer);
    if (buffer_append_n(self->currentLineBuffer, &self->lookAhead, 0) < 0)
      throwError(self, "Error appending line buffer");
    if (buffer_compact(self->currentLineBuffer) < 0)
      throwError(self, "Error clearing line buffer");
  }

  return 0;
}

static void getChar(struct lexer* self) {
  if (!self->isFirstToken && feof(self->input) == 0)
    append(self, self->lookAhead);

  int res = getCharRaw(self);
  if (res >= 0)
    return;

  if (res == -ENAVAIL)
    throwError(self, "No data left to read");
  throwError(self, "Unknown failure: %d", res);
} 

// Return white character skipped (including EOF)
static int skipWhite(struct lexer* self) {
  int count = 0;
  while (isspace((unsigned char) self->lookAhead)) {
    count++;
    
    int res = getCharRaw(self);
    if (res >= 0)
      continue;
    
    if (res == -ENAVAIL)
      return count;
    throwError(self, "Unknown failure: %d", res);
  }
  
  return count;
}

static char matchNoSkipWhite(struct lexer* self, char c) {
  if (self->lookAhead != c)
    throwError(self, "Expected '%c' got '%c'", c, self->lookAhead);

  getChar(self);
  return c;
}

static int64_t getInteger(struct lexer* self) {
  size_t current = 0;
  
  // Fixed 20 bytes because it 
  // always return int64_t and
  // maximum int64_t consumed
  // 20 bytes of space in string
  // form (including null terminator)
  char buffer[20] = {};
  
  if (!isdigit((unsigned char) self->lookAhead))
    throwError(self, "Expected 'integer'");

  char digit = self->lookAhead;
  while (isdigit((unsigned char) digit)) {
    getChar(self);
    digit = self->lookAhead;

    if (current + 1 >= sizeof(buffer))
      throwError(self, "Integer is too large");

    buffer[current] = digit;
    buffer[current + 1] = '\0';
    current++;
  }

  errno = 0;
  intmax_t n = strtoimax(buffer, NULL, 10);
  if (errno == ERANGE || n > INT64_MAX)
    throwError(self, "Integer is overflowing");

  return (int64_t) n;
}

static bool isIdentifierFirstLetter(char chr) {
  if (isalpha((unsigned char) chr))
    return true;

  switch (chr) {
    case '_':
      return true;
    case '@':
      return true;
  }

  return false;
}

static bool isIdentifier(char chr) {
  if (isalnum((unsigned char) chr))
    return true;

  switch (chr) {
    case '_':
      return true;
    case '@':
      return true;
  }

  return false;
}

static char* getIdentifier(struct lexer* self) {
  if (!isIdentifierFirstLetter(self->lookAhead))
    throwError(self, "Expected 'identifier'");
 
  size_t len = 1;
  char* buffer = malloc(2);
  if (!buffer)
    throwError(self, "Out of memory");
  buffer[0] = self->lookAhead;
  buffer[1] = '\0';

  while (isIdentifier(self->lookAhead)) {
    buffer = realloc(buffer, len + 2);
    if (!buffer)
      throwError(self, "Out of memory");
    buffer[len] = self->lookAhead;
    buffer[len + 1] = '\0';
    len++;

    getChar(self);
  }

  return buffer;
}

static char* getLabelRef(struct lexer* self) {
  matchNoSkipWhite(self, '=');
  return getIdentifier(self);
}

static char* getDirectiveName(struct lexer* self) {
  matchNoSkipWhite(self, '.');
  return getIdentifier(self);
}

static int64_t getImmediate(struct lexer* self) {
  matchNoSkipWhite(self, '#');
  return getInteger(self);
}

static char* getComment(struct lexer* self) {
  matchNoSkipWhite(self, '/');
  
  size_t len = 0;
  char* buffer = malloc(1);
  if (!buffer)
    throwError(self, "Out of memory");
  buffer[0] = '\0';

  switch (self->lookAhead) {
    // Single line comment
    case '/':
      matchNoSkipWhite(self, '/');
      while (self->lookAhead != '\n') {
        matchNoSkipWhite(self, self->lookAhead);

        buffer = realloc(buffer, len + 2);
        if (!buffer)
          throwError(self, "Out of memory");
        buffer[len] = self->lookAhead;
        buffer[len + 1] = '\0';
        len++;
      }
      matchNoSkipWhite(self, '\n');
      break;
    /* Multi line comment */
    case '*':
      matchNoSkipWhite(self, '*');
      while (true) {
        char current = self->lookAhead;
        matchNoSkipWhite(self, self->lookAhead);

        buffer = realloc(buffer, len + 2);
        if (!buffer)
          throwError(self, "Out of memory");
        buffer[len] = current;
        buffer[len + 1] = '\0';
        len++;
        
        // End of multiline comment
        if (current == '/' && buffer[len - 2] == '*') {
          // Replace '*' at the end with NUL
          buffer[len - 2] = '\0';
          break;
        }
      }
      break;
    default:
      throwError(self, "Expect single line or multiline comment");
  }

  return buffer;
}

static char* getString(struct lexer* self) {
  matchNoSkipWhite(self, '\"');
  
  size_t len = 0;
  char* buffer = malloc(1);
  if (!buffer)
    throwError(self, "Out of memory");
  buffer[0] = '\0';

  while (self->lookAhead != '\"') {
    char current = self->lookAhead;
    matchNoSkipWhite(self, self->lookAhead);

    buffer = realloc(buffer, len + 2);
    if (!buffer)
      throwError(self, "Out of memory");
    buffer[len] = current;
    buffer[len + 1] = '\0';
    len++;
  }
    
  matchNoSkipWhite(self, '\"');
  return buffer;
}

static int64_t getRegister(struct lexer* self) {
  matchNoSkipWhite(self, '$');
  matchNoSkipWhite(self, 'r');
  int64_t registerID = getInteger(self);
  if (registerID >= VM_MAX_REGISTERS)
    throwError(self, "Invalid register!");
  return registerID;
}

static void process(struct lexer* self) {
  if (isIdentifierFirstLetter(self->lookAhead)) {
    self->currentToken.type = TOKEN_IDENTIFIER;
    self->currentToken.data.identifier = getIdentifier(self);

    if (self->lookAhead == ':') {
      matchNoSkipWhite(self, ':');
      self->currentToken.type = TOKEN_LABEL_DECL;
    }
    goto exit_common;
  }

  switch (self->lookAhead) {
    case '$':
      self->currentToken.type = TOKEN_REGISTER;
      self->currentToken.data.reg = getRegister(self);
      goto exit_common;
    case '=':
      self->currentToken.type = TOKEN_LABEL_REF;
      self->currentToken.data.labelName = getLabelRef(self);
      goto exit_common;
    case '.':
      self->currentToken.type = TOKEN_DIRECTIVE_NAME;
      self->currentToken.data.directiveName = getDirectiveName(self);
      goto exit_common;
    case ',':
      self->currentToken.type = TOKEN_COMMA;
      matchNoSkipWhite(self, ',');
      goto exit_common;
    case '#':
      self->currentToken.type = TOKEN_IMMEDIATE;
      self->currentToken.data.immediate = getImmediate(self);
      goto exit_common;
    case '/':
      self->currentToken.type = TOKEN_COMMENT;
      self->currentToken.data.comment = getComment(self);
      goto exit_common;
    case '\"':
      self->currentToken.type = TOKEN_STRING;
      self->currentToken.data.string = getString(self);
      goto exit_common;
  }

  throwError(self, "Unknown token");
  
exit_common:
  return;
}

int lexer_process(struct lexer* self, struct token* result) {
  int res = 0;
  if (setjmp(self->onError) != 0) {
    res = -EFAULT;
    goto lexer_failure;
  }
  
  const char* filename = self->inputName;
  
  if (self->isFirstToken) {
    getChar(self);
    skipWhite(self);
  }
  self->isFirstToken = false;
  
  // Move start position
  self->startColumn = self->currentColumn;
  self->startLine = self->currentLine;

  self->currentToken = (struct token) {};
  
  process(self);
  self->currentToken.rawToken = self->currentTokenBuffer;
  self->currentToken.rawTokenSize = self->currentTokenBuffer->len;
  self->currentToken.startColumn = self->startColumn; 
  self->currentToken.startLine = self->startLine; 
  self->currentToken.endColumn = self->prevColumn; 
  self->currentToken.endLine = self->prevLine;
  self->currentToken.filename = filename;
  
  if (skipWhite(self) == 0)
    throwError(self, "Expected whitespace");
  
  if (result)
    *result = self->currentToken;

lexer_failure:
  if (self->currentTokenBuffer)
    buffer_free(self->currentTokenBuffer);
  self->currentTokenBuffer = NULL;
  return res; 
}




