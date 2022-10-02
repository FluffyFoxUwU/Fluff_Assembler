#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "code_emitter.h"
#include "lexer.h"
#include "vm_types.h"

int main2() {
  // struct lexer* lexer = lexer_new(stdin, "<stdin>");
  // struct token token = {};
  
  // while (feof(lexer->input) == 0) {
  //   int res = lexer_process(lexer, &token);
  //   printf("Result: %d\n", res);
  //   if (res < 0)
  //     break;

  //   printf("Token type: %d\n", token.type);
  //   printf("Token start: %d, %d\n", token.startLine, token.startColumn);
  //   printf("Token end: %d, %d\n", token.endLine, token.endColumn);
  //   printf("Token raw: '%s'\n", token.rawToken);
  //   printf("Token size: '%zu'\n", token.rawTokenSize);
     
  //   lexer_free_token(&token);
  // };

  // printf("%s\n", lexer->errorMessage);
  // lexer_free(lexer); 

  struct code_emitter* emitter = code_emitter_new();
  
  struct code_emitter_label* label = code_emitter_label_new(emitter, (struct token) {
    .filename = "<stdin>"
  });
  
  code_emitter_label_define(emitter, label);
  
  code_emitter_emit_ldint(emitter, 0x00, 0, -32);
  code_emitter_emit_ldint(emitter, 0x00, 1, 14);
  code_emitter_emit_add(emitter, 0x00, 0, 0, 1);
  
  code_emitter_emit_jmp(emitter, 0x00, label);
  
  int res;
  if ((res = code_emitter_finalize(emitter)) == -EFAULT) {
    printf("Code Emitter Error: %s\n", emitter->errorMessage);
    _exit(0);
  }
  printf("Emitter result: %d\n", res); 
  
  for (vm_instruction_pointer i = 0; i < emitter->instructionCount; i++) {
    vm_instruction ins = emitter->instructions[i];
    printf("Ins: 0x%016lx\n", ins);
  }
  
  code_emitter_free(emitter);

  _exit(0);
  return EXIT_SUCCESS;
}

