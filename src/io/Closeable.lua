local Util = require("util.Util")
local Throwable = require("Throwable")

local M = Util.newModule({})

function M.getName()
  return "Closeable"
end

local proto = {}
local MT = {
  __index = proto
}

function proto:isClosed()
  return self._closed
end

function proto:close()
  self:checkState()
  self.upcast:close()
  self._closed = true
end

function proto:checkState()
  if self._closed then
    error(Throwable.new("use after close"))
  end
end

function M.newAbstract(impl)
  Util.checkArg(1, impl, "table")

  Util.validateImplementation("Closeable#close", impl.close)

  Util.validateImplementation("Closeable#asCloseable", impl.asCloseable)

  local data = Util.newObject({
    upcast = impl,
    _closed = false
  }, MT, M)
  data._this = data
  return data
end

return M

