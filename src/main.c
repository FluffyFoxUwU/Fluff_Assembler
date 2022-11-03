#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>

#include "assembler_driver.h"

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

int main2(int argc, char** argv) {
  if (argc < 3) {
    printf("Usage: %s <source> <result>\n", argv[0]);
    return EXIT_FAILURE;
  }
  
  int exitRes = EXIT_SUCCESS;
  const char* inputFile = argv[1];
  const char* outputFile = argv[2];
  FILE* input = NULL;
  FILE* output = NULL;
  
  if ((input = fopen(inputFile, "r")) == NULL) {
    printf("Cannot open input file!");
    exitRes = EXIT_FAILURE;
    goto input_open_failure;
  }
  
  if ((output = fopen(outputFile, "w")) == NULL) {
    printf("Cannot open output file!");
    exitRes = EXIT_FAILURE;
    goto output_open_failure;
  }

  void* compiled = NULL;
  size_t compiledSize = 0;
  const char* errorMessage = NULL;
  
  clock_t startClock = clock();
  int res = assembler_driver_assemble(inputFile, input, &errorMessage, &compiled, &compiledSize);
  double cpuTime = ((double) clock() - (double) startClock) / CLOCKS_PER_SEC;
  printf("Compiling took %.2lf miliseconds\n", cpuTime * 1000);
  
  if (res < 0) {
    printf("Assembler failure: %s\n", errorMessage);
    free((char*) errorMessage);
    exitRes = EXIT_FAILURE;
    goto assemble_failure;
  }
  
  // Write compiled code
  printf("Writing %zu bytes of bytecode\n", compiledSize);
  if (fwrite(compiled, 1, compiledSize, output) != compiledSize)
    goto write_result_failure;
  
  free(compiled);

write_result_failure:
assemble_failure:
  fclose(output);
output_open_failure:
  fclose(input);
input_open_failure:
  return exitRes;
}

