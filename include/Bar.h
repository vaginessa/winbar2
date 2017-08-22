#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <windows.h>
#include <windowsx.h>

#include "Block.h"

class Bar {
public:
    Bar(std::vector<Block*>* blocks, std::mutex* blockMutex);
    ~Bar();

    void redraw();
    void setFont(std::string font, int fontSize);

    void _windowThread(); // don't use

private:
    std::thread thread;
    std::vector<Block*>* blocks;
    std::mutex* blockMutex;

    std::mutex apiMutex;
    bool shouldRedraw;
    bool shouldChangeFont;
    std::string newFont;
    int newFontSize;

    HWND winbarWnd;
    HCURSOR cursor;
    HFONT font;
    int width, height;
    bool running;

    static LRESULT CALLBACK static_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void triggerEvent(int eventType, int xCoord);
    static void printError();
};
