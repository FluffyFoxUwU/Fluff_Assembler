local Util = require("util.Util")
local Opcodes = require("Opcodes")
local Throwable = require("Throwable")
local JSON = require("lib.JSON")
local Stack = require("Stack")

local M = Util.newModule({})
function M.getName()
  return "CodeGen"
end

local proto = {}
local MT = {
  __index = proto
}

local function isValidCond(reg)
  return math.type(reg) == "integer" and
         reg >= 0 and
         reg <= 0xFF
end

local function checkCond(reg)
  if not isValidCond(reg) then
    error(Throwable.new("Invalid conditional flag '%s'"):format(tostring))
  end
end

local function isValidShort(reg)
  return math.type(reg) == "integer" and
         reg >= 0 and
         reg <= 0xFFFF
end

local function checkShort(reg)
  if not isValidShort(reg) then
    error(Throwable.new(("Invalid short flag '%s'"):format(tostring(reg))))
  end
end

local function emitInstruction(self, prototype, op, cond, A, B, C, ...)
  if not Opcodes.isValid(op) then
    error(Throwable.new("invalid opcode passed"))
  end

  checkCond(cond)

  if A then
    checkShort(A)
  end

  if B then
    checkShort(B)
  end

  if C then
    checkShort(C)
  end

  prototype.instructions[prototype.pc + 1] = {
    op = op.op,
    cond = cond,
    A = A or 0,
    B = B or 0,
    C = C or 0
  }

  prototype.lineInfo[prototype.pc] = self._currentLine
  prototype.pc = prototype.pc + 1

  if select("#", ...) > 0 then
    emitInstruction(self, prototype, Opcodes.Extra, 0x00, ...)
  end
end

function proto:emitGetConstant(cond, res, const)
  checkCond(cond)
  checkShort(res)
  Util.checkArg(3, const, "table", "nil")

  if self._constantToIndex[const] == nil then
    error(Throwable.new("unknown constant"))
  end

  emitInstruction(self, self._currentPrototype, Opcodes.GetConstant, cond, res, self._constantToIndex[const])
end

for _, key in ipairs(Opcodes.getOpcodesList()) do
  local op = Opcodes[key]

  if not proto["emit"..key] then
    proto["emit"..key] = function(self, cond, ...)
      if select("#", ...) > op.fieldsUsed then
        error(Throwable.new(("Extra unused fields for %s"):format(op.nameInString)))
      elseif select("#", ...) < op.fieldsUsed then
        error(Throwable.new(("Not enough fields for %s"):format(op.nameInString)))
      end

      checkCond(cond)
      local args = {...}
      for i = 1, select("#", ...) do
        checkShort(args[i])
      end

      emitInstruction(self, self._currentPrototype, op, cond, ...)
    end
  end
end
proto.emitExtra = nil

function proto:addConstant(val)
  if self._constantsLength >= 0xFFFF then
    error(Throwable.new("Number of constants limit reached"))
  end

  Util.checkArg(1, val, "string", "number")
  local dataType = type(val)
  if type(val) == "number" then
    dataType = math.type(val)
  end

  local const = {
    type = dataType,
    data = val
  }
  self._constants[self._constantsLength] = const
  self._constantToIndex[const] = self._constantsLength
  self._constantsLength = self._constantsLength + 1

  return const
end

function proto:startPrototype(sourceName)
  Util.checkArg(1, sourceName, "string")

  local prototype = {
    instructions = {},
    prototypes = {},

    lineInfo = {},
    pc = 0,
    sourceName = sourceName
  }

  self._currentPrototype = prototype
  self._prototypeStack:push(prototype)
end

function proto:_endPrototype()
  local prototype = self._prototypeStack:pop()
  self._currentPrototype = self._prototypeStack:peek()

  if self._currentPrototype then
    table.insert(self._currentPrototype.prototypes, prototype)
  end
end

function proto:endPrototype()
  if self._prototypeStack:peek() == self._topLevelPrototype then
    error(Throwable.new("attempt to close top level prototype"))
  end

  self:_endPrototype()
end

function proto:setLine(line)
  Util.checkArg(1, line, "number")
  self._currentLine = line
end

function proto:ensureNotDone()
  if self._done then
    error(Throwable.new("Reusing codegen instance is not valid"))
  end
end

function proto:done(writer)
  Util.checkArg(1, writer, "Writer")

  if self._prototypeStack:peek() ~= self._topLevelPrototype then
    error(Throwable.new("attempt to call CodeGen#done while a prototype havent closed"))
  end

  self:_endPrototype()

  local constants = {}
  local mainPrototype = nil

  -- Process prototype
  local function processPrototype(current)
    local processedPrototype = {
      prototypes = {},
      lineInfo = {},
      sourceFile = current.sourceName,
      instructions = {}
    }

    for pos, value in ipairs(current.instructions) do
      processedPrototype.instructions[pos] = {
        high = value.op << 24 | value.cond << 16 | value.A,
        low = value.B << 16 | value.C
      }
    end

    for k, v in pairs(current.lineInfo) do
      processedPrototype.lineInfo[k + 1] = v
    end

    for _, prototype in ipairs(current.prototypes) do
      table.insert(processedPrototype.prototypes, processPrototype(prototype))
    end

    return processedPrototype
  end
  mainPrototype = processPrototype(self._topLevelPrototype)

  -- Process constants
  for i = 0, self._constantsLength - 1 do
    constants[i + 1] = {
      type = self._constants[i].type,
      data = self._constants[i].data
    }
  end

  -- Writing result
  writer:write(JSON.encode({
    constants = constants,
    mainPrototype = mainPrototype
  }))
  self:ensureNotDone()
end

function proto:getWriter()
  return self._writer
end

function M.new(sourceName)
  Util.checkArg(1, sourceName, "string")

  local data = Util.newObject({
    _done = false,
    _prototypeStack = Stack.new(512),

    _constants = {},
    _constantToIndex = {},
    _constantsLength = 0,

    _topLevelPrototype = nil,
    _currentPrototype = nil,

    _currentLine = 0
  }, MT, M)
  data._this = data

  local self = data
  self:startPrototype(sourceName)
  self._topLevelPrototype = self._prototypeStack:peek()

  return data
end

return M

