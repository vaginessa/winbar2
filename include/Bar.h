#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <windows.h>

#include "Block.h"

class Bar {
public:
    Bar(std::vector<Block*>* blocks, std::mutex* blockMutex);
    ~Bar();
    updateBlocks();

    void _windowThread();

private:
    std::thread thread;
    std::vector<Block*>* blocks;
    std::mutex* blockMutex;

    HWND winbarWnd;
    HCURSOR cursor;
    HFONT font;
    int width, height;

    static LRESULT CALLBACK static_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static void printError();
};
