local Util = require("util.Util")
local Reader = require("io.Writer")
local Throwable = require("Throwable")

local M = Util.newModule({})

local proto = {}
local MT = {
  __index = proto
}

function M.getName()
  return "FileReader"
end

function proto:read(size)
  Util.checkArg(1, size, "number")
  return self._file:read(size)
end

function proto:close()
  self._file:close()
end

function proto:flush()
  self._file:flush()
end

function proto:asReader()
  return self._asReaderInstance
end

function proto:getFileHandle()
  return self._file
end

-- Constructors

local function commonConstructor(file)
  local data = Util.newObject({
    _file = file
  }, MT, M)

  data._this = data
  data._asReaderInstance = Reader.newAbstract(data)
  return data
end

function M.new(filename)
  Util.checkArg(1, filename, "string")

  local fp, err = io.open(filename, "r")
  if fp == nil then
    error(Throwable.new(("Cannot open file: %s"):format(err)))
  end

  return commonConstructor(fp)
end

return M



