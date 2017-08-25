#pragma once

#include <string>
#include <queue>
#include <windows.h>

#define BLOCK_EVENT_MOUSE_DOWN 1
#define BLOCK_EVENT_MOUSE_UP   2

class Block {
public:
    Block(int id);
    int id;
    std::wstring text;
    COLORREF color;
    int _rx, _width;
    int luaEventRef;
    bool hidden;

    std::queue<int> events;
};
