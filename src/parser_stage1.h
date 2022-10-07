#ifndef header_1664459362_935cdd2a_7314_4d62_b9c1_bbdb1a323dac_parser_stage1_h
#define header_1664459362_935cdd2a_7314_4d62_b9c1_bbdb1a323dac_parser_stage1_h

#include <stdbool.h>
#include <stddef.h>

#include "lexer.h"
#include "vec.h"

// Stage 1 Parser_stage1 (Combine tokens into statements)

enum statement_types {
  STATEMENT_UNKNOWN,
  STATEMENT_COMMENT,
  STATEMENT_ASSEMBLER_DIRECTIVE,
  STATEMENT_INSTRUCTION,
  STATEMENT_LABEL_DECLARE
};

struct statement {
  vec_t(struct token) tokens;
};

struct bytecode;
struct parser_stage1 {
  struct lexer* currentLexer;
  
  bool canFreeErrorMsg;
  const char* errormsg;
  
  struct token* currentToken;
};

struct parser_stage1* parser_stage1_new();
void parser_stage1_free(struct parser_stage1* self);

// 0 on success (the result at self->result)
// Errors:
// -EFAULT: Error parsing (check self->errormsg)
// -ENOMEM: Not enough memory
int parser_stage1_process(struct parser_stage1* self, struct lexer* lexer);

#endif

