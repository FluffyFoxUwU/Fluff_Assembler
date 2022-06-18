local Util = require("util.Util")
local Writer = require("io.Writer")
local Throwable = require("Throwable")

local M = Util.newModule({})

local proto = {}
local MT = {
  __index = proto
}

function M.getName()
  return "FileWriter"
end

function proto:write(data)
  self._file:write(data)
end

function proto:close()
  self._file:close()
end

function proto:flush()
  self._file:flush()
end

function proto:asWriter()
  return self._asWriterInstance
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
  data._asWriterInstance = Writer.newAbstract(data)
  return data
end

function M.new(filename)
  Util.checkArg(1, filename, "string")

  local fp, err = io.open(filename, "w")
  if fp == nil then
    error(Throwable.new(("Cannot open file: %s"):format(err)))
  end

  return commonConstructor(fp)
end

M.stdout = commonConstructor(io.stdout)
M.stderr = commonConstructor(io.stderr)

return M



