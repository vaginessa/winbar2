#pragma once

#include <string>
#include <queue>

#define BLOCK_EVENT_MOUSE_DOWN 1
#define BLOCK_EVENT_MOUSE_UP   2

class Block {
public:
    Block();
    std::wstring text;
    uint32_t color;
    int _rx, _width;
    int luaEventRef;

    std::queue<int> events;
};
