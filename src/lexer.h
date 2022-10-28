#ifndef header_1664370723_b4e2d9a3_18aa_480b_8b90_6f19fc9e5a98_lexer_h
#define header_1664370723_b4e2d9a3_18aa_480b_8b90_6f19fc9e5a98_lexer_h

#include "buffer.h"
#include "util.h"
#include "vec.h"
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#define TOKEN_TYPE \
  X(TOKEN_UNKNOWN, "unknown") \
  X(TOKEN_REGISTER, "register") \
  X(TOKEN_IMMEDIATE, "immediate") \
  X(TOKEN_IDENTIFIER, "identifier") \
  X(TOKEN_LABEL_REF, "label reference") \
  X(TOKEN_LABEL_DECL, "label declare") \
  X(TOKEN_COMMENT, "comment") \
  X(TOKEN_DIRECTIVE_NAME, "assembler directive name") \
  X(TOKEN_COMMA, "comma") \
  X(TOKEN_STRING, "string") \
  X(TOKEN_STATEMENT_END, "end of statement")

enum token_type {
# define X(name, ...) name,
  TOKEN_TYPE
# undef X
};

struct token {
  const char* filename;
  int startColumn;
  int startLine;
  int endColumn;
  int endLine;

  enum token_type type;
  
  buffer_t* rawToken;
  buffer_t* fullLine;

  union {
    int reg;
    int64_t immediate;

    buffer_t* labelName;
    buffer_t* string;
    buffer_t* labelDeclName;
    buffer_t* directiveName;
    buffer_t* identifier;
    buffer_t* comment;
  } data;
};

struct lexer {
  int startLine;
  int startColumn;
  int currentLine;
  int currentColumn;
  int prevLine;
  int prevColumn;

  bool isFirstToken;
  bool isThrowingError;
  bool isEOF;
  bool isCompleted;

  char lookAhead;

  const char* inputName;
  FILE* input;

  bool canFreeErrorMessage;
  const char* errorMessage;

  jmp_buf onError;

  buffer_t* currentTokenBuffer;
  struct token* currentToken;
  
  buffer_t* currentLineBuffer;
  
  vec_t(buffer_t*) allLines;
  vec_t(struct token*) allTokens;
  vec_t(runnable_block) cleanups;
};

struct lexer* lexer_new(FILE* input, const char* inputName);
void lexer_free(struct lexer* self);

// 0 on success
//
// Errors:
// -ENOMEM: Insufficient memory to process
// -EFAULT: Lexing error
// -EINVAL: Call on errored lexer instance
int lexer_process(struct lexer* self);

const char* lexer_get_token_name(enum token_type type);

#endif





