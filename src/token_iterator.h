#ifndef _headers_1666490570_Fluff_Assembler_statement_iterator
#define _headers_1666490570_Fluff_Assembler_statement_iterator

struct statement;
struct token;

struct token_iterator {
  struct statement* statement;
  
  struct token** tokens;
  struct token* current;
  int pointer;
  int numTokens;
};

struct token_iterator* token_iterator_new(struct statement* statement);
void token_iterator_free(struct token_iterator* self);

// Errors: 
// -ENODATA: No more token to read
int token_iterator_next(struct token_iterator* self, struct token** token);

// Errors:
// -EINVAL: Invalid next token
// -ENODATA: No more token to read
int token_iterator_next_identifier(struct token_iterator* self, const char** ident);

#endif

