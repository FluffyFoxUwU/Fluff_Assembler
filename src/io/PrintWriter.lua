local Util = require("util.Util")

local M = Util.newModule({})

function M.getName()
  return "PrintWriter"
end

local proto = {}
local MT = {
  __index = proto
}

function proto:println(strOrNil)
  Util.checkArg(1, strOrNil, "string", "nil")
  if strOrNil == nil then
    self._writer:write("\n")
  else
    self:print(strOrNil)
    self:println()
  end
end

function proto:print(str)
  Util.checkArg(1, str, "string")
  self._writer:write(str)
end

function proto:getWriter()
  return self._writer
end

function M.new(writer)
  Util.checkArg(1, writer, "Writer")

  local data = Util.newObject({
    _writer = writer
  }, MT, M)

  data._this = data
  return data
end

return M

