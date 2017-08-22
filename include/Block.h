#pragma once

#include <string>

class Block {
public:
    Block();
    std::wstring text;
    uint32_t color;
    int _rx, _width;
};
