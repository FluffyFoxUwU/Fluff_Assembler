#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include "lexer.h"
#include "parser_stage2.h"
#include "statement_compiler.h"
#include "code_emitter.h"
#include "bytecode/bytecode.h"
#include "token_iterator.h"
#include "util.h"
#include "opcodes.h"
#include "vm_types.h"

struct entry {
  const char* name;
  statement_processor_func processor;
  int condType;
};

static void setErr(struct statement_processor_context* ctx, bool canFreeErr, const char* err) {
  ctx->err = err;
  ctx->canFreeErr = canFreeErr;
}

static int commonReadNext(struct statement_processor_context* ctx, struct token** token) {
  if (token_iterator_next(ctx->stage2Context->iterator, token) == -ENODATA) {
    setErr(ctx, false, "statement_processor: No token to read");
    return -EFAULT; 
  }
  
  return 0;
}

static int typeCheckedReadNext(struct statement_processor_context* ctx, struct token** token, enum token_type type) {
  static const char* lookup[] = {
#   define X(t, str, ...) [t] = "statement_processor: Expecting '" str "'",
    TOKEN_TYPE
#   undef X
  };
  
  int res = commonReadNext(ctx, token);
  if (res < 0)
    return res;
  
  if ((*token)->type != type) {
    if (type < 0 || type > ARRAY_SIZE(lookup)) 
      setErr(ctx, false, __FILE__ ": Out of bound lookup[i] access please report");
    else
      setErr(ctx, false, lookup[type]);
    return -EFAULT;
  }
  
  return 0;
}

// Return 0 on sucess otherwise negative errno
// Errors:
// -EFAULT: Error occured
// static int getInteger(struct statement_processor_context* ctx, int64_t* integer) {
//   struct token* token;
//   if (typeCheckedReadNext(ctx, &token, TOKEN_IMMEDIATE))
//     return -EFAULT;
  
//   *integer = token->data.immediate;
//   return 0;
// }

// Return register ID on sucess otherwise negative errno
// Errors:
// -EFAULT: Error occured
static int getRegister(struct statement_processor_context* ctx) {
  struct token* token;
  if (typeCheckedReadNext(ctx, &token, TOKEN_REGISTER))
    return -EFAULT;
  
  return token->data.reg;
}

// Return 0 on sucess otherwise negative errno
// Errors:
// -EFAULT: Error occured
// -ENOMEM: Out of memory
static int getLabel(struct statement_processor_context* ctx, struct code_emitter_label** label) {
  struct token* token;
  if (typeCheckedReadNext(ctx, &token, TOKEN_LABEL_REF))
    return -EFAULT;
  
  int res = 0;
  if (label)
    res = parser_stage2_get_label(ctx->stage2Context, buffer_string(token->data.labelName), label);
  return res;
}

static int slowIntegerLdr(struct statement_processor_context* ctx, int reg, int64_t integer) {
  if (integer < VM_INT_MIN || integer > VM_INT_MAX) {
    setErr(ctx, false, "Integer is larger than largest supported integer");
    return -EFAULT;
  }

  int constIndex = bytecode_add_constant_int(ctx->owner->parser->bytecode, (vm_int) integer);
  if (constIndex < 0)
    return constIndex;
  return code_emitter_emit_ldconst(ctx->stage2Context->emitter, ctx->funcEntry->udata1, reg, constIndex);
}

static int fastIntegerLdr(struct statement_processor_context* ctx, int reg, int64_t integer) {
  return code_emitter_emit_ldint(ctx->stage2Context->emitter, ctx->funcEntry->udata1, reg, (int32_t) integer);
}

static int stringLdr(struct statement_processor_context* ctx, int reg, const char* string) {
  int constIndex = bytecode_add_constant_string(ctx->owner->parser->bytecode, string);
  if (constIndex < 0)
    return constIndex;
  return code_emitter_emit_ldconst(ctx->stage2Context->emitter, ctx->funcEntry->udata1, reg, constIndex);
}

static int prototypeLdr(struct statement_processor_context* ctx, int reg, const char* string) {
  int64_t prototypeTemporaryIndex = parser_stage2_get_prototype_id(ctx->stage2Context, string);
  if (prototypeTemporaryIndex < 0)
    return (int) prototypeTemporaryIndex;
  return code_emitter_emit_impldep1(ctx->stage2Context->emitter, ctx->funcEntry->udata1, reg, (uint32_t) prototypeTemporaryIndex);
}

static int ins_ldr(struct statement_processor_context* ctx) {
  int res = 0;
  int reg = getRegister(ctx);
  if (reg < 0)
    return reg; 
  
  struct token* token = NULL;
  if ((res = commonReadNext(ctx, &token)) < 0)
    return res;
  
  switch (token->type) {
    case TOKEN_IMMEDIATE:
      // Inline the integer into the instruction if possible
      // else use slow one which loads from constant pool
      if (token->data.immediate >= INT32_MIN && token->data.immediate <= INT32_MAX)
        res = fastIntegerLdr(ctx, reg, token->data.immediate);
      else
        res = slowIntegerLdr(ctx, reg, token->data.immediate);
      break;
    case TOKEN_STRING:
      res = stringLdr(ctx, reg, buffer_string(token->data.string));
      break;
    case TOKEN_LABEL_REF:
      res = prototypeLdr(ctx, reg, buffer_string(token->data.string));
      break;
    default:
      setErr(ctx, false, "ins_ldr: Unknown second operand");
      res = -EFAULT;
      break;
  }

  return res;
}

#define macro_two_reg_ins(name, emitter_func) \
  static int name(struct statement_processor_context* ctx) { \
    int a = getRegister(ctx); \
    if (a < 0) \
      return a; \
    int b = getRegister(ctx); \
    if (b < 0) \
      return b; \
     \
    return emitter_func(ctx->stage2Context->emitter, ctx->funcEntry->udata1, a, b); \
  }
#define macro_three_reg_ins(name, emitter_func) \
  static int name(struct statement_processor_context* ctx) { \
    int a = getRegister(ctx); \
    if (a < 0) \
      return a; \
    int b = getRegister(ctx); \
    if (b < 0) \
      return b; \
    int c = getRegister(ctx); \
    if (c < 0) \
      return c; \
     \
    return emitter_func(ctx->stage2Context->emitter, ctx->funcEntry->udata1, a, b, c); \
  }
#define macro_no_arg_ins(name, emitter_func) \
  static int name(struct statement_processor_context* ctx) { \
    return emitter_func(ctx->stage2Context->emitter, ctx->funcEntry->udata1); \
  }

macro_no_arg_ins(ins_nop, code_emitter_emit_nop);
macro_no_arg_ins(ins_ret, code_emitter_emit_ret);

macro_two_reg_ins(ins_cmp, code_emitter_emit_cmp);
macro_two_reg_ins(ins_mov, code_emitter_emit_mov);

macro_three_reg_ins(ins_add, code_emitter_emit_add);
macro_three_reg_ins(ins_sub, code_emitter_emit_sub);
macro_three_reg_ins(ins_mul, code_emitter_emit_mul);
macro_three_reg_ins(ins_div, code_emitter_emit_div);
macro_three_reg_ins(ins_mod, code_emitter_emit_mod);
macro_three_reg_ins(ins_pow, code_emitter_emit_pow);

#undef macro_no_arg_ins
#undef macro_three_reg_ins
#undef macro_two_reg_ins

static int ins_b(struct statement_processor_context* ctx) {
  struct code_emitter_label* label = NULL;
  int res = getLabel(ctx, &label);
  if (res < 0)
    return res;
  
  return code_emitter_emit_jmp(ctx->stage2Context->emitter, ctx->funcEntry->udata1, label);
}

static struct entry instructions[] = {
# define instruction(name, func) \
  {name, func, OP_COND_AL}, \
  {name ".al", func, OP_COND_AL}, \
  {name ".eq", func, OP_COND_EQ}, \
  {name ".lt", func, OP_COND_LT}, \
  {name ".ne", func, OP_COND_NE}, \
  {name ".gt", func, OP_COND_GT}, \
  {name ".ge", func, OP_COND_GE}, \
  {name ".le", func, OP_COND_LE}, \
  
  // Pseudo instructions
  // It can be ldint or ldconst based on argument
  instruction("ldr", ins_ldr)
  
  // No arg instructions
  instruction("nop", ins_nop)
  instruction("ret", ins_ret)
  
  // Single label instructions
  instruction("b", ins_b)
  
  // Two regs instructions
  instruction("cmp", ins_cmp)
  instruction("mov", ins_mov)
  
  // Three regs instructions
  instruction("add", ins_add)
  instruction("sub", ins_sub)
  instruction("mul", ins_mul)
  instruction("div", ins_div)
  instruction("mod", ins_mod)
  instruction("pow", ins_pow)
# undef instruction
};

int default_processor_register(struct statement_compiler* compiler, struct parser_stage2* stage2) {
  int res = 0;
  bool status[ARRAY_SIZE(instructions)] = {};
  
  for (int i = 0; i < ARRAY_SIZE(instructions); i++) {
    struct statement_processor processor = {
      .processor = instructions[i].processor,
      .udata = stage2,
      
      // Store what cond type current instruction is
      .udata1 = instructions[i].condType
    };
    
    if ((res = statement_compiler_register(compiler, instructions[i].name, &processor)) < 0)
      goto register_failure;
    status[i] = true;
  }
  
register_failure:
  assert(res != -EEXIST); /* This shouldnt fail on properly tested code */
  
  if (res < 0)
    for (int i = 0; i < ARRAY_SIZE(instructions); i++)
      if (status[i])
        // TODO: Do something if this fails for whatever reason
        statement_compiler_unregister(compiler, instructions[i].name);
  
  return res;
}

void default_processor_unregister(struct statement_compiler* compiler, struct parser_stage2* stage2) {
  for (int i = 0; i < ARRAY_SIZE(instructions); i++)
    statement_compiler_unregister(compiler, instructions[i].name);
}
