local Util = require("util.Util")

local M = Util.newModule({})
function M.getName()
  return "Supplier"
end

local proto = {}
local MT = {
  __index = proto
}

function proto:get()
  return self._supplier()
end

function proto:forEach(consumer)
  for item in self._supplier do
    consumer:consume(item)
  end
end

function M.new(supplier)
  Util.checkArg(1, supplier, "function")

  local data = Util.newObject({
    _supplier = supplier
  }, MT, M)

  data._this = data
  return data
end

return M

