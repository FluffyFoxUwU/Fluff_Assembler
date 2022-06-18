#!/bin/sh
ROOT=$(dirname $(realpath "$0"))
export LUA_PATH="$LUA_PATH;$ROOT/src/?.lua;$ROOT/libs/?.lua"
export LUA_CPATH="$LUA_CPATH;$ROOT/libs/?.so"

lua54 $ROOT/src/main.lua "$@"
exit $?
