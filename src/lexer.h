#ifndef header_1664370723_b4e2d9a3_18aa_480b_8b90_6f19fc9e5a98_lexer_h
#define header_1664370723_b4e2d9a3_18aa_480b_8b90_6f19fc9e5a98_lexer_h

#include "buffer.h"
#include "vec.h"
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

enum token_type {
  TOKEN_UNKNOWN,
  TOKEN_REGISTER,
  TOKEN_IMMEDIATE,
  TOKEN_IDENTIFIER,
  TOKEN_LABEL_REF,
  TOKEN_LABEL_DECL,
  TOKEN_COMMENT,
  TOKEN_DIRECTIVE_NAME,
  TOKEN_COMMA,
  TOKEN_STRING,
  TOKEN_STATEMENT_END // End of statement essentially is `;`
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
    int64_t reg;
    int64_t immediate;

    buffer_t* labelName;
    buffer_t* string;
    buffer_t* labelDeclName;
    buffer_t* directiveName;
    buffer_t* identifier;
    buffer_t* comment;
  } data;
};

void lexer_free_token(struct token* token);

typedef void (^lexer_onerror_cleanup_block)();
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
  vec_t(lexer_onerror_cleanup_block) cleanups;
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

#endif





