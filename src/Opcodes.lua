local M = {}

local function newInstruction(op, nameInString, fieldUsed)
  return {
    name = nameInString,
    fieldsUsed = fieldUsed,
    op = op
  }
end

M.Nop = newInstruction(0x00, "nop", 0)
M.Mov = newInstruction(0x01, "mov", 2)
M.TableGet = newInstruction(0x02, "table_get", 3)
M.TableSet = newInstruction(0x03, "table_set", 3)
M.Call = newInstruction(0x04, "call", 4)
M.StackPush = newInstruction(0x05, "stack_push", 1)
M.StackPop = newInstruction(0x06, "stack_pop", 1)
M.GetConstant = newInstruction(0x07, "get_constant", 2)
M.Return = newInstruction(0x08, "ret", 2)
M.Extra = newInstruction(0x09, "extra", 3)
M.StackGetTop = newInstruction(0x0A, "stack_gettop", 1)
M.LoadPrototype = newInstruction(0x0B, "load_prototype", 2)
M.Add = newInstruction(0x0C, "add", 3)
M.Sub = newInstruction(0x0D, "sub", 3)
M.Mul = newInstruction(0x0E, "mul", 3)
M.Div = newInstruction(0x0F, "div", 3)
M.Mod = newInstruction(0x10, "mod", 3)
M.Pow = newInstruction(0x11, "pow", 3)
M.JmpForward = newInstruction(0x12, "jmp_forward", 1)
M.JmpBackward = newInstruction(0x13, "jmp_backward", 1)
M.Cmp = newInstruction(0x14, "cmp", 2)

local opcodesList = {}
local opcodesLookup = {}

for k, v in pairs(M) do
  table.insert(opcodesList, k)
  opcodesLookup[v] = k
end

function M.getOpcodesList()
  return opcodesList
end

function M.isValid(op)
  return opcodesLookup[op] ~= nil
end

return M





