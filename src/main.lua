local System = require("System")
local FileWriter = require("io.FileWriter")
local CodeGen = require("CodeGen")
local Opcodes = require("Opcodes")

local function main(argc)
  local inputFilename
  local outputFilename
  if #argc < 2 then
    System.stderr:print(([[Usage:

%s <input assembly> <output bytecode>
]]):format(argc[0]))
    return 1
  end
  inputFilename = argc[1]
  outputFilename = argc[2]

  local bytecodeWriter = FileWriter.new(outputFilename)
  local gen = CodeGen.new(inputFilename)

  local global = {
    const = function(val)
      gen:setLine(debug.getinfo(2, "l").currentline)
      return gen:addConstant(val)
    end,

    start_prototype = function()
      gen:setLine(debug.getinfo(2, "l").currentline)
      gen:startPrototype(debug.getinfo(2, "S").short_src or "<unknown>")
    end,

    end_prototype = function()
      gen:setLine(debug.getinfo(2, "l").currentline)
      gen:endPrototype(debug.getinfo(2, "S").short_src or "<unknown>")
    end
  }

  for _, key in ipairs(Opcodes.getOpcodesList()) do
    local op = Opcodes[key]
    global[op.name] = function(cond, ...)
      gen:setLine(debug.getinfo(2, "l").currentline)
      gen["emit"..key](gen, cond, ...)
    end
  end

  local begin = os.clock()
  local f, err = loadfile(inputFilename, "t", global)
  if not f then
    System.stderr:println(("Cannot load '%s': %s"):format(inputFilename, err))
  end
  f()

  gen:done(bytecodeWriter:asWriter())
  bytecodeWriter:close()

  local timeSpent = os.clock() - begin
  System.stdout:println(("Compilation took %.2f ms"):format(timeSpent * 1000))
end

local _, exit = xpcall(main, function(msg)
  System.stderr:print("Uncatched exception caught: ")
  if type(msg) == "table" then
    msg:printStacktrace(System.stderr)
  else
    System.stderr:println(msg or "(error object is nil)")
    System.stderr:println(debug.traceback())
  end

  return 0
end, arg)
os.exit(exit, true)

