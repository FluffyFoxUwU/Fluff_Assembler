local Util = require("util.Util")

local M = Util.newModule({})
function M.getName()
  return "Throwable"
end

local proto = {}
local MT = {
  __index = proto
}

function proto:getMessage()
  return self._message
end

function proto:toString()
  return ("%s: %s"):format(Util.getObjectName(self._this), self:getMessage())
end

function proto:getStacktrace()
  return self._stacktrace
end

function proto:getCause()
  return self._cause
end

function proto:printStacktrace(pw)
  Util.checkArg(1, pw, "PrintWriter")
  pw:println(self:toString())
  pw:println(self:getStacktrace())

  if self:getCause() then
    pw:print("Caused by: ")
    self:getCause():printStacktrace(pw)
  end
end

-- Constructors

function M.newWithMessageAndCause(message, cause)
  Util.checkArg(1, message, "string")
  Util.checkArg(2, cause, "Throwable", "nil")

  local data = Util.newObject({
    _stacktrace = debug.traceback(),
    _message = message,
    _cause = cause
  }, MT, M)
  data._this = data

  return data
end

function M.new(message)
  return M.newWithMessageAndCause(message, nil)
end

function M.newWithCause(cause)
  Util.checkArg(1, cause, "Throwable", "nil")
  local msg
  if cause then
    msg = cause:getMessage()
  else
    msg = "(no message)"
  end

  return M.newWithMessageAndCause(msg, cause)
end

return M

