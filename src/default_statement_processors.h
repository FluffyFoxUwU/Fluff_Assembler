#ifndef _headers_1666428869_Fluff_Assembler_default_statement_processors
#define _headers_1666428869_Fluff_Assembler_default_statement_processors

struct statement_compiler;
struct parser_stage2;

// On error the effects is reverted
// Error code forwarded from statement_compiler_register
int default_processor_register(struct statement_compiler* compiler, struct parser_stage2* stage2);

void default_processor_unregister(struct statement_compiler* compiler, struct parser_stage2* stage2);

#endif

