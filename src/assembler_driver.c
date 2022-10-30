#include <errno.h>
#include <string.h>

#include "assembler_driver.h"
#include "bytecode/bytecode.h"
#include "bytecode/protobuf_serializer.h"
#include "code_emitter.h"
#include "lexer.h"
#include "parser_stage1.h"
#include "parser_stage2.h"
#include "vec.h"
#include "vm_types.h"

int assembler_driver_assemble(const char* inputName, FILE* inputFile, const char** errorMessageRet, void** resultRet, size_t* sizeRet) {
  int res = 0;
  struct lexer* lexer = NULL;
  struct bytecode* bytecode = NULL;
  struct parser_stage1* parser_stage1 = NULL;
  struct parser_stage2* parser_stage2 = NULL;
  char* errorMessage = NULL;
  
  if ((lexer = lexer_new(inputFile, inputName)) == NULL) {
    res = -ENOMEM;
    goto lexer_alloc_failure;
  }
  
  if ((parser_stage1 = parser_stage1_new(lexer)) == NULL) {
    res = -ENOMEM;
    goto stage1_alloc_failure;
  }
  
  if ((parser_stage2 = parser_stage2_new(parser_stage1)) == NULL) {
    res = -ENOMEM;
    goto stage2_alloc_failure;
  } 
  
  if ((res = lexer_process(lexer)) < 0) {
    errorMessage = strdup(lexer->errorMessage);
    goto lexer_failure;
  }
  
  if ((res = parser_stage1_process(parser_stage1)) < 0) {
    errorMessage = strdup(parser_stage1->errorMessage);
    goto stage1_failure;
  }
  
  if ((parser_stage2_process(parser_stage2, &bytecode)) < 0) {
    errorMessage = strdup(parser_stage2->errorMessage);
    goto stage2_failure;
  }
  
  // Only serialize when needed
  if (!resultRet)
    goto serialization_unneded;
  
  void* result;
  size_t size;
  res = bytecode_serializer_protobuf(bytecode, &result, &size);
  
  if (res < 0)
    goto serializing_failure;
  
  *sizeRet = size;
  *resultRet = result;

serializing_failure:
serialization_unneded:
  bytecode_free(bytecode);
stage1_failure:
stage2_failure:
lexer_failure:
  parser_stage2_free(parser_stage2);
stage2_alloc_failure:
  parser_stage1_free(parser_stage1);
stage1_alloc_failure:
  lexer_free(lexer);
lexer_alloc_failure:
  
  if (errorMessageRet)
    *errorMessageRet = errorMessage;
  return res;
}
