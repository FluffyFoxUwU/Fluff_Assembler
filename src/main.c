#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "code_emitter.h"
#include "lexer.h"
#include "parser_stage1.h"
#include "parser_stage2.h"
#include "vec.h"
#include "vm_types.h"

/*
Pipeline for source to bytecode generation

Bytecode
   |
ProtoBuf
   |
Parser Stage 2
   |
Parser Stage 1
   |
 Lexer
   |
Source file
*/

int main2() {
  const char* file = "../example.fluff";
  FILE* fp = fopen(file, "r");
  if (!fp) {
    puts("Cannot open file!");
    return EXIT_FAILURE;
  }
  
  struct lexer* lexer = lexer_new(fp, file);
  struct parser_stage1* parser_stage1 = parser_stage1_new(lexer);
  struct parser_stage2* parser_stage2 = parser_stage2_new(parser_stage1);
  
  int res = lexer_process(lexer);
  fclose(fp);
  
  if (res < 0) {
    printf("Lexer error: %s (errno %d)\n", lexer->errorMessage, res);
    goto failure;
  }
  
  res = parser_stage1_process(parser_stage1);
  if (res < 0) {
    printf("Stage1 parser error: %s (errno %d)\n", parser_stage1->errorMessage, res);
    goto failure;
  }
  
  res = parser_stage2_process(parser_stage2);
  if (res < 0) {
    printf("Stage2 parser error: %s (errno %d)\n", parser_stage2->errorMessage, res);
    goto failure;
  }
  
  failure:
  parser_stage2_free(parser_stage2);
  parser_stage1_free(parser_stage1);
  lexer_free(lexer);
  
  /*
  struct code_emitter* emitter = code_emitter_new();
  struct code_emitter_label* label = code_emitter_label_new(emitter, (struct token) {
    .filename = "<test.fluff>"
  });
  
  code_emitter_emit_jmp(emitter, 0x00, label);
  code_emitter_emit_add(emitter, 0x00, 0, 0, 0);
  code_emitter_emit_add(emitter, 0x00, 0, 0, 0);
  code_emitter_emit_add(emitter, 0x00, 0, 0, 0);
  code_emitter_label_define(emitter, label);
  
  int res;
  if ((res = code_emitter_finalize(emitter)) < 0)
    printf("emitter error: %s\n", emitter->errorMessage);
  
  vm_instruction ins = 0;
  int i = 0;
  vec_foreach(&emitter->instructions, ins, i)
    printf("Ins: 0x%lx\n", ins);
  code_emitter_free(emitter);
  */
  return EXIT_SUCCESS;
}

