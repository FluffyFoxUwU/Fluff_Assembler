#include <stdlib.h>
#include <stdio.h>

#include "lexer.h"

int main2() {
  struct lexer* lexer = lexer_new(stdin, "<stdin>");
  struct token token = {};
  
  while (feof(lexer->input) == 0) {
    int res = lexer_process(lexer, &token);
    printf("Result: %d\n", res);
    if (res < 0)
      break;

    printf("Token type: %d\n", token.type);
    printf("Token start: %d, %d\n", token.startLine, token.startColumn);
    printf("Token end: %d, %d\n", token.endLine, token.endColumn);
    printf("Token raw: '%s'\n", token.rawToken);
    printf("Token size: '%zu'\n", token.rawTokenSize);
     
    lexer_free_token(&token);
  };

  printf("%s\n", lexer->errorMessage);
  fflush(stdout);
  lexer_free(lexer);
  _exit(0);
  return EXIT_SUCCESS;
}

