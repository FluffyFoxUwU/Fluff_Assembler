local FileWriter = require("io.FileWriter")
local PrintWriter = require("io.PrintWriter")
local Util = require("util.Util")

local M = Util.newModule({})

function M.getName()
  return "System"
end

M.stdout = PrintWriter.new(FileWriter.stdout:asWriter())
M.stderr = PrintWriter.new(FileWriter.stderr:asWriter())

return M

