#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "code_emitter.h"
#include "lexer.h"
#include "parser.h"
#include "vm_types.h"

int main2() {
  struct lexer* lexer = lexer_new(stdin, "<stdin>");
  struct parser* parser = parser_new();
  
  if (parser_process(parser, lexer) < 0) {
    perror("parser");
    return EXIT_FAILURE;
  }
  
  parser_free(parser);
  lexer_free(lexer); 
  return EXIT_SUCCESS;
}

