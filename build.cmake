# Build config

set(BUILD_PROJECT_NAME "Fluff Assembler")

# We're making library
set(BUILD_IS_LIBRARY NO)

# If we want make libary and
# executable project
set(BUILD_INSTALL_EXECUTABLE YES)

# Sources which common between exe and library
set(BUILD_SOURCES
  src/dummy.c
  src/lexer.c
  src/util.c
  src/bytecode/bytecode.c
  src/bytecode/prototype.c
  src/code_emitter.c
  src/common.c
  src/parser.c
  deps/buffer/buffer.c
  deps/vec/vec.c
)

set(BUILD_INCLUDE_DIRS 
  ./deps/buffer
  ./deps/vec
)

# Note that exe does not represent Windows' 
# exe its just short hand of executable 
#
# Note:
# Still creates executable even building library. 
# This would contain test codes if project is 
# library. The executable directly links to the 
# library objects instead through shared library
set(BUILD_EXE_SOURCES
  src/specials.c
  src/premain.c
  src/main.c
)

# Public header to be exported
# If this a library
set(BUILD_PUBLIC_HEADERS
  include/dummy.h
)

set(BUILD_PROTOBUF_FILES
  src/format/bytecode.proto
)

set(BUILD_CFLAGS "")
set(BUILD_LDFLAGS "")

# AddPkgConfigLib is in ./buildsystem/CMakeLists.txt
macro(AddDependencies)
  # Example
  # AddPkgConfigLib(FluffyGC FluffyGC>=1.0.0)
  
  link_libraries(-lm -lprotobuf-c)
endmacro()


