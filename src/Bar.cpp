#include "Bar.h"

#include <stdio.h>

void Bar::triggerEvent(int eventType, int xCoord) {
    blockMutex->lock();

    // Find block index
    int width = this->width;
    int blockIdx;
    for (blockIdx = blocks->size() - 1; blockIdx >= 0; blockIdx--) {
        if (xCoord >= width - blocks->at(blockIdx)->_width && xCoord < width) {
            break;
        } else {
            width -= blocks->at(blockIdx)->_width;
        }
    }
    if (blockIdx < 0) return;

    // Add to event queue
    blocks->at(blockIdx)->events.push(eventType);

    blockMutex->unlock();
}

LRESULT CALLBACK Bar::static_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Bar* bar = (Bar*)GetWindowLong(hwnd, GWLP_USERDATA);
    if (!bar) return DefWindowProc(hwnd, msg, wParam, lParam);
    return bar->wndproc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK Bar::wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PAINT) {
        // Get size
        RECT drawRect;
        drawRect.left = 0;
        drawRect.top = 0;
        drawRect.right = width;
        drawRect.bottom = height;

        // Begin
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Clear
        SelectObject(hdc, GetStockObject(BLACK_BRUSH));
        Rectangle(hdc, drawRect.left, drawRect.top, drawRect.right, drawRect.bottom);

        // Text options
        SetBkColor(hdc, 0);
        SelectObject(hdc, font);

        // Draw blocks
        SIZE textSize;
        Block* block;
        blockMutex->lock();
        for (int blockIdx = blocks->size() - 1; blockIdx >= 0; blockIdx--) {
            block = blocks->at(blockIdx);

            // Draw
            SetTextColor(hdc, block->color);
            DrawTextW(hdc, block->text.c_str(), -1, &drawRect, DT_SINGLELINE | DT_NOCLIP | DT_RIGHT | DT_VCENTER | DT_NOPREFIX);

            // Place next text
            GetTextExtentPoint32W(hdc, block->text.c_str(), block->text.length(), &textSize);
            block->_rx = drawRect.right;
            block->_width = textSize.cx;
            drawRect.right -= textSize.cx;
        }
        blockMutex->unlock();

        // End
        EndPaint(hwnd, &ps);
        return 0;
    } else if (msg == WM_DESTROY) {
        // todo: die
    } else if (msg == WM_SETCURSOR) {
        SetCursor(cursor);
    } else if (msg == WM_LBUTTONDOWN) {
        triggerEvent(BLOCK_EVENT_MOUSE_DOWN, GET_X_LPARAM(lParam));
    } else if (msg == WM_LBUTTONUP) {
        triggerEvent(BLOCK_EVENT_MOUSE_UP, GET_X_LPARAM(lParam));
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Get SetLayeredWindowAttributes function
typedef BOOL (WINAPI* SLWAProc)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);
BOOL SetLayeredWindowAttributes_COLORKEY(HWND hwnd) {
    SLWAProc proc = (SLWAProc)GetProcAddress(GetModuleHandle("USER32.DLL"), "SetLayeredWindowAttributes");
    return proc(hwnd, 0, 0, 1); // LWA_COLORKEY = 1
}

// Print win32 errors
void Bar::printError() {
    wchar_t buf[256];
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 256, NULL);
    printf("%ls\n", buf);
}

// Thread main that creates window and handles messages
void Bar::_windowThread() {
    // Load defaults
    font = CreateFont(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0, "Arial");
    cursor = LoadCursor(NULL, IDC_ARROW);

    // Find taskbar
    HWND trayWnd = FindWindowEx(0, 0, "Shell_TrayWnd", 0);
    HWND taskbarWnd = FindWindowEx(trayWnd, 0, "ReBarWindow32", 0);
    if (!taskbarWnd) {
        printf("Failed to find taskbar!\n");
        return;
    }

    // Get taskbar size
    RECT rc;
    GetWindowRect(taskbarWnd, &rc);
    int taskbarWidth = rc.right - rc.left;
    int taskbarHeight = rc.bottom - rc.top;

    // Create class
    WNDCLASSEX wc;
    memset(&wc, 0, sizeof(WNDCLASSEX));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = static_wndproc;
    wc.lpszClassName = "winbarC";
    wc.style = CS_VREDRAW | CS_HREDRAW;

    // Register
    if (!RegisterClassEx(&wc)) {
        printf("Failed to register class!\n");
        printError();
        return;
    }

    // Create window
    winbarWnd = CreateWindowEx(0, "winbarC", "", 0, 0, 0, 100, 100, 0, 0, 0, 0);
    if (!winbarWnd) {
        printf("Failed to create window!\n");
        printError();
        return;
    }
    SetWindowLong(winbarWnd, GWLP_USERDATA, (long)this);

    // Make a child
    SetParent(winbarWnd, taskbarWnd);
    LONG style = GetWindowLong(winbarWnd, GWL_STYLE);
    style &= ~(WS_POPUP | WS_CAPTION);
    style |= WS_CHILD;
    SetWindowLong(winbarWnd, GWL_STYLE, style);

    // Set colorkey properties
    SetLayeredWindowAttributes_COLORKEY(winbarWnd);

    // Show window
    ShowWindow(winbarWnd, SW_SHOW);
    UpdateWindow(winbarWnd);

    // Resize it
    width = taskbarWidth / 2;
    height = taskbarHeight;
    SetWindowPos(winbarWnd, 0, taskbarWidth - width, taskbarHeight - height, width, height, 0);

    // Start window loop
    MSG msg;
    while (1) { // todo: don't run forever
        // Handle messages
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // todo: See if taskbar got resized

        apiMutex.lock(); // API
        // Font change
        if (shouldChangeFont) {
            DeleteObject(font);
            font = CreateFont(newFontSize, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0, newFont.c_str());
            shouldRedraw = true;
            shouldChangeFont = false;
        }

        // Redraw if needed
        if (shouldRedraw) {
            RedrawWindow(winbarWnd, 0, 0, RDW_UPDATENOW | RDW_INVALIDATE);
            shouldRedraw = false;
        }
        apiMutex.unlock(); // /API

        // Chill
        Sleep(1);
    }

    // Destroy objects
    DeleteObject(font);
    DeleteObject(cursor);

    return;
}

void windowThreadDelegate(Bar* po) {
    po->_windowThread();
}

void Bar::redraw() {
    apiMutex.lock();
    shouldRedraw = true;
    apiMutex.unlock();
}

void Bar::setFont(std::string font, int fontSize) {
    apiMutex.lock();
    shouldChangeFont = true;
    newFont = font;
    newFontSize = fontSize;
    apiMutex.unlock();
}

Bar::Bar(std::vector<Block*>* blocks, std::mutex* blockMutex) {
    this->blocks = blocks;
    this->blockMutex = blockMutex;
    thread = std::thread(windowThreadDelegate, this);
}

Bar::~Bar() {
    thread.join();
}
