local M = {}

function M.getName()
  return "Util"
end

function M.validateImplementation(name, func)
  M.checkArg(1, name, "string")
  M.checkArg(2, func, "function", "nil")

  if func == nil then
    error(("%s not implemented"):format(name));
  end
end

-- Modified from:
-- https://github.com/MightyPirates/OpenComputers/blob/af2db43c53b9690fceabfb813987572bf2258db5/src/main/resources/assets/opencomputers/lua/machine.lua#L65
--
function M.checkArg(n, have, ...)
  local haveType = type(have)
  local haveType2 = nil
  if haveType == "table" and have._type then
    haveType2 = have._type.getName()
  end

  local function check(want, ...)
    if not want then
      return false
    else
      return haveType == want or
             haveType2 == want or
             check(...)
    end
  end

  if not check(...) then
    local msg = string.format("bad argument #%d (%s expected, got %s)",
                              n, table.concat({...}, " or "), have)
    error(msg)
  end
end

function M.typeof(data, type0)
  M.checkArg(1, data, "table")
  M.checkArg(2, data, "table")

  return data._type == type0
end

local function toStringObject(self)
  local instance = self
  while instance and instance.upcast do
    instance = instance.upcast
  end

  if instance.toString then
    local str = instance:toString()
    if type(str) ~= "string" then
      error("cannot happen")
    end
    return str
  end

  if not instance._type then
    return ("%s %p"):format(type(instance), instance)
  end

  return ("%s %p"):format(M.getObjectName(instance), instance)
end

function M.getObjectName(object)
  M.checkArg(1, object, "table")

  local instance = object
  while instance and instance.upcast do
    instance = instance.upcast
  end

  return instance._type.getName()
end

function M.newObject(data, meta, type0)
  return setmetatable({
    _type = type0
  }, {
    __index = setmetatable(data, meta),
    __tostring = toStringObject
  })
end

local MT = {
  __tostring = function (self)
    return self:getName()
  end
}

function M.newModule(def)
  return setmetatable(def, MT)
end

function M.rethrow(err)
  M.checkArg(1, err, "Throwable")
  error(err)
end

M = M.newModule(M)
return M

