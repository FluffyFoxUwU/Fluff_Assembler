#!/bin/sh
export LUA_PATH="$LUA_PATH;./src/?.lua;./libs/?.lua"
export LUA_CPATH="$LUA_CPATH;./libs/?.so"
lua54 ./src/main.lua "$@"
exit $?
