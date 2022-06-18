local Util = require("util.Util")
local Closeable = require("io.Closeable")

local M = Util.newModule({})

local proto = {}
local MT = {
  __index = proto
}

function M.getName()
  return "Reader"
end

function proto:close()
  self.upcast:close()
end

function proto:flush()
  self:asCloseable():checkState()
  self.upcast:flush()
end

function proto:read(size)
  self:asCloseable():checkState()
  return self.upcast:read(size)
end

function proto:asCloseable()
  return self._asCloseable
end

function M.newAbstract(impl)
  Util.checkArg(1, impl, "table")

  Util.validateImplementation("Reader#close", impl.close)
  Util.validateImplementation("Reader#read", impl.read)
  Util.validateImplementation("Reader#flush", impl.flush)

  Util.validateImplementation("Reader#asReader", impl.asReader)

  local data = Util.newObject({
    upcast = impl
  }, MT, M)

  data._this = data
  data._asCloseable = Closeable.newAbstract(data)

  return data
end

return M

