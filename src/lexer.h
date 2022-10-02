#ifndef header_1664370723_b4e2d9a3_18aa_480b_8b90_6f19fc9e5a98_lexer_h
#define header_1664370723_b4e2d9a3_18aa_480b_8b90_6f19fc9e5a98_lexer_h

#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

enum token_type {
  TOKEN_REGISTER,
  TOKEN_IMMEDIATE,
  TOKEN_IDENTIFIER,
  TOKEN_LABEL_REF,
  TOKEN_LABEL_DECL,
  TOKEN_COMMENT,
  TOKEN_DIRECTIVE_NAME,
  TOKEN_COMMA,
  TOKEN_STRING,
  TOKEN_UNKNOWN
};

struct token {
  const char* filename;
  int startColumn;
  int startLine;
  int endColumn;
  int endLine;

  enum token_type type;
  
  size_t rawTokenSize;
  const char* rawToken;

  union {
    int64_t reg;
    int64_t immediate;

    const char* labelName;
    const char* string;
    const char* labelDeclName;
    const char* directiveName;
    const char* identifier;
    const char* comment;
  } data;
};

void lexer_free_token(struct token* token);

struct lexer {
  int startLine;
  int startColumn;
  int currentLine;
  int currentColumn;
  int prevLine;
  int prevColumn;

  bool isFirstToken;
  bool isThrowingError;

  char lookAhead;

  const char* inputName;
  FILE* input;

  bool canFreeErrorMessage;
  const char* errorMessage;

  jmp_buf onError;

  size_t currentTokenBufferSize;
  char* currentTokenBuffer;

  struct token currentToken;
  
  // Used for context in error message
  size_t currentLineBufferSize;
  char* currentLineBuffer;
};

struct lexer* lexer_new(FILE* input, const char* inputName);
void lexer_free(struct lexer* self);

// 0 on success
//
// Errors:
// -ENOMEM: Insufficient memory to process
// -EFAULT: Lexing error
// -EINVAL: Call on errored lexer instance
//
// Note on failure: `result` content is undefined
int lexer_process(struct lexer* self, struct token* result);

#endif





