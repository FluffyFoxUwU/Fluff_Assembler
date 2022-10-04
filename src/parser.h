#ifndef header_1664459362_935cdd2a_7314_4d62_b9c1_bbdb1a323dac_parser_h
#define header_1664459362_935cdd2a_7314_4d62_b9c1_bbdb1a323dac_parser_h

#include <stdbool.h>
#include <stddef.h>

#include "lexer.h"

struct bytecode;
struct parser {
  struct lexer* currentLexer;
  
  bool canFreeErrorMsg;
  const char* errormsg;
  
  struct bytecode* result;
  struct token currentToken; 
};

struct parser* parser_new();
void parser_free(struct parser* self);

// 0 on success (the result at self->result)
// Errors:
// -EFAULT: Error parsing (check self->errormsg)
// -ENOMEM: Not enough memory
int parser_process(struct parser* self, struct lexer* lexer);

#endif

