local Util = require("util.Util")

local M = Util.newModule({})
function M.getName()
  return "Consumer"
end

local proto = {}
local MT = {
  __index = proto
}

function proto:consume(item)
  return self._consumer(item)
end

function M.new(consumer)
  Util.checkArg(1, consumer, "function")

  local data = Util.newObject({
    _consumer = consumer
  }, MT, M)

  data._this = data
  return data
end

return M

