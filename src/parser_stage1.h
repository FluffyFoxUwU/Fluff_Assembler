#ifndef header_1664459362_935cdd2a_7314_4d62_b9c1_bbdb1a323dac_parser_stage1_h
#define header_1664459362_935cdd2a_7314_4d62_b9c1_bbdb1a323dac_parser_stage1_h

#include <stdbool.h>
#include <stddef.h>

#include "buffer.h"
#include "lexer.h"
#include "vec.h"

// Stage 1 Parser_stage1 (Combine tokens into statements)

enum statement_type {
  STATEMENT_UNKNOWN,
  STATEMENT_COMMENT,
  STATEMENT_ASSEMBLER_DIRECTIVE,
  STATEMENT_INSTRUCTION,
  STATEMENT_LABEL_DECLARE
};

struct statement {
  enum statement_type type;
  vec_t(struct token*) rawTokens;
  vec_t(struct token*) wholeStatement;
  
  union {
    buffer_t* labelName;
    buffer_t* commentData;
  } data;
};

struct bytecode;
struct parser_stage1 {
  struct lexer* lexer;
  
  bool isCompleted;
  bool canFreeErrorMsg;
  bool isFirstToken;
  const char* errorMessage;
  
  struct token* currentToken;
  struct statement* currentStatement;
  
  int tokenPointer;
  vec_t(struct statement*) allStatements;
};

struct parser_stage1* parser_stage1_new(struct lexer* lexer);
void parser_stage1_free(struct parser_stage1* self);

// 0 on success
// Errors:
// -EFAULT: Error parsing (check self->errormsg)
// -ENOMEM: Not enough memory
// -EINVAL: Attempt to process failed instance
int parser_stage1_process(struct parser_stage1* self);

#endif

