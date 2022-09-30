#ifndef header_1664459362_935cdd2a_7314_4d62_b9c1_bbdb1a323dac_parser_h
#define header_1664459362_935cdd2a_7314_4d62_b9c1_bbdb1a323dac_parser_h

struct lexer;
struct parser {
  struct lexer* lexer;
};

struct parser* parser_new(struct lexer* lexer);
void parser_free(struct parser* self);

int parser_process(struct lexer* lexer);

#endif

