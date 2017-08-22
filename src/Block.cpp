#include "Block.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

Block::Block() {
    luaEventRef = LUA_NOREF;
}
