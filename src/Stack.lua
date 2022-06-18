local Util = require("util.Util")
local Supplier = require("functional.Supplier")
local Throwable= require("Throwable")

local M = Util.newModule({})

function M.getName()
  return "Stack"
end

local proto = {}
local MT = {
  __index = proto
}

function proto:getSupplier()
  local i = 0
  return Supplier.new(function()
    i = i + 1
    return self._stack[i]
  end)
end

function proto:getCapacity()
  return self._capacity
end

function proto:push(item)
  Util.checkArg(1, item, "table", "function", "userdata", "number", "boolean", "string")
  if self._sp == self:getCapacity() then
    error(Throwable.new("stack overflow"))
  end

  self._sp = self._sp + 1
  self._stack[self._sp] = item
end

function proto:peek()
  return self._stack[self._sp]
end

function proto:pop()
  if self._sp == 0 then
    error(Throwable.new("stack underflow"))
  end

  local item = self._stack[self._sp]
  self._stack[self._sp] = nil
  self._sp = self._sp - 1

  return item
end

function M.new(capacity)
  Util.checkArg(1, capacity, "number")

  local data = Util.newObject({
    _capacity = capacity,
    _sp = 0,
    _stack = {}
  }, MT, M)
  data._this = data

  return data
end

return M

