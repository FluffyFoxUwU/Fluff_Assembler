#include <Block.h>
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
#include <assert.h>

#include "buffer.h"
#include "lexer.h"
#include "constants.h"
#include "util.h"
#include "common.h"

static void freeToken(struct token* token);
struct lexer* lexer_new(FILE* input, const char* inputName) {
  struct lexer* self = malloc(sizeof(*self));
  if (!self)
    return NULL;

  self->input = input;
  self->errorMessage = NULL;
  self->canFreeErrorMessage = false;
  self->lookAhead = '\0';
  self->currentToken = NULL;
  self->currentLine = 0;
  self->currentColumn = -1;
  self->isFirstToken = true;
  self->isThrowingError = false;
  self->inputName = inputName;
  self->currentLineBuffer = NULL;
  self->currentTokenBuffer = NULL;
  self->isEOF = false;
  self->isCompleted = false;

  self->currentLineBuffer = buffer_new();
  if (!self->currentLineBuffer)
    goto failure;
  self->currentTokenBuffer = NULL;
  
  vec_init(&self->cleanups);
  vec_init(&self->allLines);
  vec_init(&self->allTokens);
  return self;

failure:
  lexer_free(self);
  return NULL;
}

void lexer_free(struct lexer* self) {
  if (self->canFreeErrorMessage)
    free((char*) self->errorMessage);
  
  int i = 0;
  buffer_t* line = NULL;
  vec_foreach(&self->allLines, line, i) {
    buffer_free(line);
  }
  
  i = 0;
  struct token* token = NULL;
  vec_foreach(&self->allTokens, token, i) {
    freeToken(token);
  }
  
  if (self->cleanups.length > 0) {
    fprintf(stderr, __FILE__ ":%d: Unreleased cleanup detected!!! (this is a bug please report)\n", __LINE__);
    abort();
  }
  
  vec_deinit(&self->allLines);
  vec_deinit(&self->allTokens);
  vec_deinit(&self->cleanups);
  
  if (self->currentTokenBuffer)
    buffer_free(self->currentTokenBuffer);
  if (self->currentLineBuffer)
    buffer_free(self->currentLineBuffer);
  free(self);
}

static void freeToken(struct token* token) {
  if (!token)
    return;
  
  switch (token->type) {
    case TOKEN_STRING:
      if (token->data.string)
        buffer_free(token->data.string);
      break;
    case TOKEN_LABEL_DECL:
      if (token->data.labelDeclName)
        buffer_free(token->data.labelDeclName);
      break;
    case TOKEN_LABEL_REF:
      if (token->data.labelName)
        buffer_free(token->data.labelName);
      break;
    case TOKEN_DIRECTIVE_NAME:
      if (token->data.directiveName)
        buffer_free(token->data.directiveName);
      break;
    case TOKEN_IDENTIFIER:
      if (token->data.identifier)
        buffer_free(token->data.identifier);
      break;
    case TOKEN_COMMENT:
      if (token->data.comment)
        buffer_free(token->data.comment);
      break;
    case TOKEN_STATEMENT_END:
    case TOKEN_REGISTER:
    case TOKEN_COMMA:
    case TOKEN_IMMEDIATE:
    case TOKEN_UNKNOWN:
      break;
  }

  if (token->rawToken)
    buffer_free(token->rawToken);
  free(token);
}

// Return 0 on success
// Errors:
// -ENOMEM: Not enough memory
static int pushErrorCleanup(struct lexer* self, runnable_block cleanupBlock) {
  runnable_block copy = Block_copy(cleanupBlock);
  if (!copy)
    return -ENOMEM;
  
  if (vec_push(&self->cleanups, copy) < 0) {
    Block_release(copy);
    return -ENOMEM;
  }
  return 0;
}

static void popErrorCleanup(struct lexer* self) {
  if (self->cleanups.length == 0) {
    fprintf(stderr, __FILE__ ":%d: Cleanup stack underflow!!! (this is a bug please report)\n", __LINE__);
    abort();
  }
  
  Block_release(vec_pop(&self->cleanups));
}

static void recordTokenInfo(struct lexer* self) {
  self->currentToken->rawToken = self->currentTokenBuffer;
  self->currentToken->fullLine = self->currentLineBuffer;
  self->currentToken->filename = self->inputName;
  
  self->currentToken->startColumn = self->startColumn; 
  self->currentToken->startLine = self->startLine; 
  
  self->currentToken->endColumn = self->prevColumn; 
  self->currentToken->endLine = self->prevLine;
}

// Return zero on success
// Errors:
// -ENAVAIL: No data left
// -EFAULT: Read error (check errno for reason)
static int getCharRaw(struct lexer* self) {
  if (self->isEOF)
    return -ENAVAIL;
  
  self->prevColumn = self->currentColumn;
  self->prevLine = self->currentLine;
  
  if (self->lookAhead == '\n') {
    // Replace current line buffer with new one
    self->currentLine++;
    self->currentColumn = -1;
    self->prevColumn--;

    if (vec_push(&self->allLines, self->currentLineBuffer) < 0)
      return -ENOMEM;

    self->currentLineBuffer = buffer_new();
    if (self->currentLineBuffer == NULL)
      return -ENOMEM;
  }
  
  // Fetch data
  if (fread(&self->lookAhead, 1, 1, self->input) != 1) {
    if (feof(self->input) != 0) {
      self->isEOF = true;
      return -ENAVAIL;
    }
    
    if (ferror(self->input) != 0)
      return -EFAULT;
    
    fputs(__FILE__ ": FATAL: Cannot read input (unknown error) (this a bug please report)\n", stderr);
    abort();
  }
  
  if (self->lookAhead != '\n') {
    // Increment to next column and append
    self->currentColumn++;
    
    if (buffer_append_n(self->currentLineBuffer, &self->lookAhead, 1) < 0) 
      return -ENOMEM;
  }
  
  int raise(int);
  printf("Yay: %c\n", self->lookAhead);
  if (self->lookAhead == ';')
    raise(5);
  return 0;
}

static void throwError_vprintf(struct lexer* self, const char* fmt, va_list args) {
  if (self->isThrowingError) {
    fputs(__FILE__ ": FATAL: Nested error!!! (this a bug please report)\n", stderr);
    abort();
  }
  
  while (self->lookAhead != '\n')
    if (getCharRaw(self) < 0)
      break;
  
  recordTokenInfo(self);
  
  char* buffer;
  util_vasprintf(&buffer, fmt, args);
  if (!buffer)
    goto format_error;

  self->isThrowingError = true;
  self->canFreeErrorMessage = true;
  self->errorMessage = common_format_error_message_about_token(self->inputName, 
                                                               "lexing error", 
                                                               self->currentLine,
                                                               self->currentColumn >= 0 ? self->currentColumn : 0,
                                                               self->currentToken,
                                                               "%s",
                                                               buffer);
  self->isThrowingError = false;
  
  if (self->errorMessage == NULL)
    goto format_error;
  free(buffer);

  runnable_block current = NULL;
  int i = 0;
  vec_foreach_rev(&self->cleanups, current, i) {
    current();
    Block_release(current);
  }
  vec_clear(&self->cleanups);

  longjmp(self->onError, 1);
  abort();
  
  format_error:
  self->canFreeErrorMessage = false;
  self->errorMessage = "Cannot format error message";

  longjmp(self->onError, 1);
  abort();
}

ATTRIBUTE_PRINTF(2, 3)
static void throwError(struct lexer* lexer, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  throwError_vprintf(lexer, fmt, args);
  va_end(args);
}

static int getCharAllowEOF(struct lexer* self) {
  if (!self->isFirstToken && 
      feof(self->input) == 0 && 
      buffer_append_n(self->currentTokenBuffer, &self->lookAhead, 1) < 0) {
    throwError(self, "Error appending token buffer");
  }

  int res = getCharRaw(self);
  if (res >= 0 || res == -ENAVAIL)
    return res;

  switch (res) {
    case -ENAVAIL:
      throwError(self, "No data left to read");
    case -EFAULT:
      throwError(self, "Read error: %d", errno);
    case -ENOMEM:
      throwError(self, "Not enough memory");
  }
  
  throwError(self, "Unknown failure: %d", res);
  return 0;
}

static void getChar(struct lexer* self) {
  int res = getCharAllowEOF(self);
  if (res >= 0)
    return;

  switch (res) {
    case -ENAVAIL:
      throwError(self, "No data left to read");
    case -EFAULT:
      throwError(self, "Read error");
  }
  
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
    case '$':
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
    case '$':
      return true;
  }

  return false;
}

static buffer_t* getIdentifier(struct lexer* self) {
  if (!isIdentifierFirstLetter(self->lookAhead))
    throwError(self, "Expected 'identifier'");
 
  buffer_t* buffer = buffer_new();
  if (!buffer)
    throwError(self, "Out of memory");
  if (pushErrorCleanup(self, ^() {
        buffer_free(buffer);
      }) < 0)
    goto error_push_cleanup;
  
  while (isIdentifier(self->lookAhead)) {
    if (buffer_append_n(buffer, &self->lookAhead, 1) < 0)
      throwError(self, "Error appending");
    
    getChar(self);
  }

  popErrorCleanup(self);
  return buffer;
  
error_push_cleanup:
  buffer_free(buffer);
  throwError(self, "Error pushing cleanup block");
  return NULL;
}

static buffer_t* getLabelRef(struct lexer* self) {
  matchNoSkipWhite(self, '=');
  return getIdentifier(self);
}

static buffer_t* getDirectiveName(struct lexer* self) {
  matchNoSkipWhite(self, '.');
  return getIdentifier(self);
}

static buffer_t* getLabelDecl(struct lexer* self) {
  matchNoSkipWhite(self, ':');
  buffer_t* ident = getIdentifier(self);
  matchNoSkipWhite(self, ':');
  return ident;
}

static int64_t getImmediate(struct lexer* self) {
  matchNoSkipWhite(self, '#');
  return getInteger(self);
}

static buffer_t* getComment(struct lexer* self) {
  matchNoSkipWhite(self, '/');
  
  buffer_t* buffer = buffer_new();
  if (!buffer)
    throwError(self, "Out of memory");
  if (pushErrorCleanup(self, ^() {
      buffer_free(buffer);
    }) < 0)
    goto error_push_cleanup;
  
  switch (self->lookAhead) {
    /* Multi line comment */
    case '*':
      matchNoSkipWhite(self, '*');
      while (true) {
        char current = self->lookAhead;
        matchNoSkipWhite(self, self->lookAhead);

        if (buffer_append_n(buffer, &current, 1) < 0)
          throwError(self, "Error appending");
        
        // End of multiline comment
        if (current == '/' && buffer->data[buffer_length(buffer) - 2] == '*') {
          // Replace '*' at the end with NUL
          buffer->data[buffer_length(buffer) - 2] = '\0';
          break;
        }
      }
      break;
    default:
      throwError(self, "Expect single line or multiline comment");
  }

  popErrorCleanup(self);
  return buffer;

error_push_cleanup:
  buffer_free(buffer);
  throwError(self, "Error pushing cleanup block");
  return NULL;
}

static buffer_t* getString(struct lexer* self) {
  matchNoSkipWhite(self, '\"');
  
  buffer_t* buffer = buffer_new();
  if (!buffer)
    throwError(self, "Out of memory");
  if (pushErrorCleanup(self, ^() {
      buffer_free(buffer);
    }) < 0)
    goto error_push_cleanup;
  
  while (self->lookAhead != '\"') {
    char current = self->lookAhead;
    matchNoSkipWhite(self, self->lookAhead);
    if (buffer_append_n(buffer, &current, 1) < 0) 
      throwError(self, "Error appending");
  }
    
  popErrorCleanup(self);
  matchNoSkipWhite(self, '\"');
  return buffer;

error_push_cleanup:
  buffer_free(buffer);
  throwError(self, "Error pushing cleanup block");
  return NULL;
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
  switch (self->lookAhead) {
    case '$':
      self->currentToken->type = TOKEN_REGISTER;
      self->currentToken->data.reg = getRegister(self);
      goto exit_common;
    case '=':
      self->currentToken->type = TOKEN_LABEL_REF;
      self->currentToken->data.labelName = getLabelRef(self);
      goto exit_common;
    case '.':
      self->currentToken->type = TOKEN_DIRECTIVE_NAME;
      self->currentToken->data.directiveName = getDirectiveName(self);
      goto exit_common;
    case ':':
      self->currentToken->type = TOKEN_LABEL_DECL;
      self->currentToken->data.labelDeclName = getLabelDecl(self);
      goto exit_common;
    case ',':
      self->currentToken->type = TOKEN_COMMA;
      matchNoSkipWhite(self, ',');
      goto exit_common;
    case '#':
      self->currentToken->type = TOKEN_IMMEDIATE;
      self->currentToken->data.immediate = getImmediate(self);
      goto exit_common;
    case '/':
      self->currentToken->type = TOKEN_COMMENT;
      self->currentToken->data.comment = getComment(self);
      goto exit_common;
    case '\"':
      self->currentToken->type = TOKEN_STRING;
      self->currentToken->data.string = getString(self);
      goto exit_common;
    case ';':
      self->currentToken->type = TOKEN_STATEMENT_END;
      matchNoSkipWhite(self, ';');
      goto exit_common;
  }

  printf("yy: %c\n", self->lookAhead);
  throwError(self, "Unknown token");
  
exit_common:
  return;
}

// volatile because longjmp and setjmp
static int lexer_process_one(struct lexer* self, struct token** result) {
  self->currentToken = malloc(sizeof(*self->currentToken));
  if (self->currentToken == NULL)
    return -ENOMEM;
  *self->currentToken = (struct token) {};
  
  int res = 0;
  if (setjmp(self->onError) != 0) {
    res = -EFAULT;
    goto lexer_failure;
  }
  
  self->currentTokenBuffer = buffer_new();
  if (self->currentTokenBuffer == NULL)
    throwError(self, "Error creating token buffer");
  
  if (self->isFirstToken) {
    self->isFirstToken = false;
    
    getChar(self);
    skipWhite(self);
  }
  
  // Move start position
  self->startColumn = self->currentColumn;
  self->startLine = self->currentLine;
  
  process(self);
  recordTokenInfo(self);
  self->currentTokenBuffer = NULL;
  
  skipWhite(self);
  
  // Save to token struct storage allocated earlier
  if (result)
    *result = self->currentToken;

  self->currentToken = NULL;
  self->currentTokenBuffer = NULL;
  return res;

lexer_failure:
  recordTokenInfo(self);
  
  // Also free's self->currentTokenBuffer as
  // self->currentTokenBuffer == self->currentToken->rawToken
  // at this context
  freeToken(self->currentToken);
  
  self->currentTokenBuffer = NULL;
  self->currentToken = NULL;
  return res;
}

int lexer_process(struct lexer* self) {
  if (self->errorMessage || self->isCompleted)
    return -EINVAL;
  self->isCompleted = true;
  
  struct token* currentToken = NULL;
  int res = 0;
  while (!self->errorMessage && !self->isEOF) {
    res = lexer_process_one(self, &currentToken);
    if (res < 0) 
      break;
  
    // if (currentToken->type == TOKEN_COMMENT) {
    //   freeToken(currentToken);
    //   continue;
    // }
    
    if (vec_push(&self->allTokens, currentToken) < 0)
      return -ENOMEM;
  }
  
  return res;
}



