#include <iostream>
#include <mutex>
#include <vector>
#include <chrono>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include "Bar.h"
#include "Block.h"

struct timer {
    long long time;
    int luaRef;
};

int blockIdCC = 10000;
Bar* bar;
std::vector<Block*> blocks;
std::mutex blockMutex;
std::vector<timer> timers;

// https://stackoverflow.com/questions/10737644/convert-const-char-to-wstring
std::wstring s2ws(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo( size_needed, 0 );
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

long long mstime() {
    std::chrono::duration<double, std::milli> ms = std::chrono::high_resolution_clock::now().time_since_epoch();
    return ms.count();
}

#ifdef __MINGW32__
// GetConsoleWindow hack for MinGW
typedef HWND (WINAPI* GCWProc)();
HWND GetConsoleWindow() {
    GCWProc proc = (GCWProc)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "GetConsoleWindow");
    return proc();
}
#endif // __MINGW32__

int l_timer(lua_State* L) {
    if (!lua_isinteger(L, 1)) return luaL_argerror(L, 1, "not an integer");
    if (!lua_isfunction(L, 2)) return luaL_argerror(L, 2, "not a function");

    // Get function ref
    lua_pushvalue(L, 2);
    int r = luaL_ref(L, LUA_REGISTRYINDEX);

    // Schedule
    long long endTime = mstime() + lua_tointeger(L, 1);
    timers.push_back(timer{endTime, r});
    return 0;
}

int getBlockID(lua_State* L, int stackIndex) {
    lua_rawgeti(L, stackIndex, 1);
    int blockID = lua_tointeger(L, -1);
    lua_pop(L, 1);
    return blockID;
}

int getBlockIdx(int blockID) {
    blockMutex.lock();
    for (size_t i = 0; i < blocks.size(); i++) {
        if (blocks.at(i)->id == blockID) {
            blockMutex.unlock();
            return i;
        }
    }
    blockMutex.unlock();
    return -1;
}

int l_BLOCK_SetText(lua_State* L) {
    // Args
    if (!lua_istable(L, 1)) return luaL_argerror(L, 1, "not a table");
    if (!lua_isstring(L, 2)) return luaL_argerror(L, 2, "not a string");

    // Get block index
    int blockIdx = getBlockIdx(getBlockID(L, 1));
    if (blockIdx < 0) return luaL_argerror(L, 1, "not a block");

    // Set
    blockMutex.lock();
    std::wstring str = s2ws(lua_tostring(L, 2));
    if (blocks.at(blockIdx)->text != str) {
        blocks.at(blockIdx)->text = str;
        bar->redraw();
    }
    blockMutex.unlock();
    return 0;
}

int l_BLOCK_SetColor(lua_State* L) {
    // Args
    if (!lua_istable(L, 1)) return luaL_argerror(L, 1, "not a table");
    if (!lua_isinteger(L, 2)) return luaL_argerror(L, 2, "not an integer");
    if (!lua_isinteger(L, 3)) return luaL_argerror(L, 3, "not an integer");
    if (!lua_isinteger(L, 4)) return luaL_argerror(L, 4, "not an integer");

    // Get block index
    int blockIdx = getBlockIdx(getBlockID(L, 1));
    if (blockIdx < 0) return luaL_argerror(L, 1, "not a block");

    // Set
    blockMutex.lock();
    COLORREF col = RGB(lua_tointeger(L, 2), lua_tointeger(L, 3), + lua_tointeger(L, 4));
    if (blocks.at(blockIdx)->color != col) {
        blocks.at(blockIdx)->color = col;
        bar->redraw();
    }
    blockMutex.unlock();
    return 0;
}

int l_BLOCK_SetHandler(lua_State* L) {
    // Args
    if (!lua_istable(L, 1)) return luaL_argerror(L, 1, "not a table");
    if (!lua_isfunction(L, 2)) return luaL_argerror(L, 2, "not a function");

    // Get block index
    int blockIdx = getBlockIdx(getBlockID(L, 1));
    if (blockIdx < 0) return luaL_argerror(L, 1, "not a block");

    blockMutex.lock(); // block
    // Unref previous
    int r = blocks.at(blockIdx)->luaEventRef;
    if (r != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, r);

    // Set new ref
    lua_pushvalue(L, 2);
    r = luaL_ref(L, LUA_REGISTRYINDEX);
    blocks.at(blockIdx)->luaEventRef = r;
    blockMutex.unlock(); // /block
    return 0;
}

int l_BLOCK_Delete(lua_State* L) {
    // Args
    if (!lua_istable(L, 1)) return luaL_argerror(L, 1, "not a table");

    // Get block index
    int blockIdx = getBlockIdx(getBlockID(L, 1));
    if (blockIdx < 0) return luaL_argerror(L, 1, "not a block");

    // Delete
    blockMutex.lock();
    delete blocks.at(blockIdx);
    blocks.erase(blocks.begin() + blockIdx);
    blockMutex.unlock();

    // Redraw
    bar->redraw();
    return 0;
}

int l_BLOCK_Show(lua_State* L) {
    // Args
    if (!lua_istable(L, 1)) return luaL_argerror(L, 1, "not a table");

    // Get block index
    int blockIdx = getBlockIdx(getBlockID(L, 1));
    if (blockIdx < 0) return luaL_argerror(L, 1, "not a block");

    // Set
    blockMutex.lock();
    if (blocks.at(blockIdx)->hidden) {
        blocks.at(blockIdx)->hidden = false;
        bar->redraw();
    }
    blockMutex.unlock();
    return 0;
}

int l_BLOCK_Hide(lua_State* L) {
    // Args
    if (!lua_istable(L, 1)) return luaL_argerror(L, 1, "not a table");

    // Get block index
    int blockIdx = getBlockIdx(getBlockID(L, 1));
    if (blockIdx < 0) return luaL_argerror(L, 1, "not a block");

    // Set
    blockMutex.lock();
    if (!blocks.at(blockIdx)->hidden) {
        blocks.at(blockIdx)->hidden = true;
        bar->redraw();
    }
    blockMutex.unlock();
    return 0;
}

int l_BAR_CreateBlock(lua_State* L) {
    // Create a block and add to list
    blockMutex.lock();
    int blockID = blockIdCC++;
    blocks.push_back(new Block(blockID));
    blockMutex.unlock();

    // Create Block object
    // todo: learn the correct way to do this (metatables or something)
    lua_newtable(L);
    lua_pushinteger(L, 1);
    lua_pushinteger(L, blockID); // todo: use userdata here instead of simply index 1
    lua_settable(L, -3);
    lua_pushstring(L, "SetText");
    lua_pushcfunction(L, l_BLOCK_SetText);
    lua_settable(L, -3);
    lua_pushstring(L, "SetColor");
    lua_pushcfunction(L, l_BLOCK_SetColor);
    lua_settable(L, -3);
    lua_pushstring(L, "SetHandler");
    lua_pushcfunction(L, l_BLOCK_SetHandler);
    lua_settable(L, -3);
    lua_pushstring(L, "Delete");
    lua_pushcfunction(L, l_BLOCK_Delete);
    lua_settable(L, -3);
    lua_pushstring(L, "Show");
    lua_pushcfunction(L, l_BLOCK_Show);
    lua_settable(L, -3);
    lua_pushstring(L, "Hide");
    lua_pushcfunction(L, l_BLOCK_Hide);
    lua_settable(L, -3);

    // Redraw
    bar->redraw();
    return 1;
}

int l_BAR_SetFont(lua_State* L) {
    // Args
    if (!lua_isstring(L, 1)) return luaL_argerror(L, 1, "not a string");
    if (!lua_isinteger(L, 2)) return luaL_argerror(L, 2, "not an integer");

    // Set
    bar->setFont(lua_tostring(L, 1), lua_tointeger(L, 2));
    return 0;
}

int l_BAR_ClearBlocks(lua_State* L) {
    // Delete
    blockMutex.lock();
    for (size_t i = 0; i < blocks.size(); i++) delete blocks.at(i);
    blocks.clear();
    blockMutex.unlock();

    // Redraw
    bar->redraw();
    return 0;
}

int handleLuaError(lua_State* L, int result) {
    if (result) {
        std::string errormsg = "Error running winbar.lua:\n    ";
        errormsg.append(lua_tostring(L, -1));
        MessageBoxA(0, errormsg.c_str(), "winbar2", 16);
        return -1;
    }
    return 0;
}

// https://www.c-plusplus.net/forum/109539-full
// Dirty copy-paste until I manually fix this
void usleep(__int64 usec) {
    HANDLE timer;
    LARGE_INTEGER ft;

    ft.QuadPart = -(10*usec); // Convert to 100 nanosecond interval, negative value indicates relative time

    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Create a new hidden console
    AllocConsole();
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    // Create file if doesn't exists
    DWORD dwAttrib = GetFileAttributes("winbar.lua");
    if (dwAttrib == INVALID_FILE_ATTRIBUTES || (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
        MessageBoxA(0, "No winbar.lua!\n", "winbar2", 16);
        return -1;
        // todo: default file
        //FILE* f = fopen("winbar.lua", "w");
        //fwrite(default_winbar_lua, default_winbar_lua_len, 1, f);
        //fclose(f);
        //printf("Created default winbar.lua\n");
    }

    // Create bar
    bar = new Bar(&blocks, &blockMutex);

    // Init lua
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    // Add global event stuff
    lua_pushinteger(L, BLOCK_EVENT_MOUSE_DOWN);
    lua_setglobal(L, "BLOCK_EVENT_MOUSE_DOWN");
    lua_pushinteger(L, BLOCK_EVENT_MOUSE_UP);
    lua_setglobal(L, "BLOCK_EVENT_MOUSE_UP");

    // Add timer global
    lua_pushcfunction(L, l_timer);
    lua_setglobal(L, "timer");

    // Add bar table
    lua_newtable(L);
    lua_pushstring(L, "SetFont");
    lua_pushcfunction(L, l_BAR_SetFont);
    lua_settable(L, -3);
    lua_pushstring(L, "CreateBlock");
    lua_pushcfunction(L, l_BAR_CreateBlock);
    lua_settable(L, -3);
    lua_pushstring(L, "ClearBlocks");
    lua_pushcfunction(L, l_BAR_ClearBlocks);
    lua_settable(L, -3);
    lua_setglobal(L, "Bar");

    // Run file
    if (handleLuaError(L, luaL_dofile(L, "winbar.lua"))) return -1;

    // Run main loop
    while (1) {
        // Handle timers
        for (int i = timers.size() - 1; i >= 0; i--) {
            if (mstime() >= timers.at(i).time) {
                int r = timers.at(i).luaRef;
                lua_rawgeti(L, LUA_REGISTRYINDEX, r);
                if (handleLuaError(L, lua_pcall(L, 0, 0, 0))) return -1;
                luaL_unref(L, LUA_REGISTRYINDEX, r);
                timers.erase(timers.begin() + i);
            }
        }

        // Block events
        // todo: handle more than one event per tick
        int r = LUA_NOREF;
        int evt = -1;
        blockMutex.lock();
        for (size_t i = 0; i < blocks.size(); i++) {
            if (!blocks.at(i)->events.empty()) {
                if (blocks.at(i)->luaEventRef == LUA_NOREF) {
                    while (!blocks.at(0)->events.empty()) blocks.at(i)->events.pop();
                } else {
                    r = blocks.at(i)->luaEventRef;
                    evt = blocks.at(i)->events.front();
                    blocks.at(i)->events.pop();
                }
            }
        }
        blockMutex.unlock();
        if (evt >= 0) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, r);
            lua_pushinteger(L, evt);
            if (handleLuaError(L, lua_pcall(L, 1, 0, 0))) return -1;
        }

        // Chill
        usleep(1);
    }

    // Close
    lua_close(L);

    return 0;
}
