#include "Block.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

Block::Block(int id) {
    this->id = id;
    text = std::wstring();
    color = 0xffffff;
    luaEventRef = LUA_NOREF;
    hidden = false;
}
