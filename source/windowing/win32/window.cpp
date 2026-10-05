#include "window_win32.hpp"
#include <input.hpp>
#include <log.hpp>
#include <math.hpp>
#include <render.hpp>

#include "../../../gfx/windows/resource.h"

/* this beautiful code is not unicode safe but i am not going to care i am so sorry */

static LRESULT CALLBACK wndproc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    WindowWin32 *self = (WindowWin32 *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (self == nullptr) return DefWindowProc(hWnd, msg, wp, lp);

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hDC = BeginPaint(hWnd, &ps);
        RECT r;

        GetClientRect(hWnd, &r);

        StretchBlt(hDC, 0, 0, r.right - r.left, r.bottom - r.top, self->hDC, 0, 0, r.right - r.left, r.bottom - r.top, SRCCOPY);
        EndPaint(hWnd, &ps);
        break;
    }
    case WM_ERASEBKGND: {
        return 1;
    }
    case WM_CLOSE: {
        self->shouldCloseFlag = 1;
        break;
    }
    case WM_QUIT: {
        self->shouldCloseFlag = 1;
        break;
    }
    default: {
        return DefWindowProc(hWnd, msg, wp, lp);
    }
    }
    return 0;
}

bool WindowWin32::init(int width, int height, const std::string &title) {
    WNDCLASSEX wc;
    HDC hDC;

    this->shouldCloseFlag = 0;

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndproc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = (HINSTANCE)GetModuleHandle(nullptr);
    wc.lpszClassName = "scratch";
    wc.lpszMenuName = nullptr;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_MENU);
    wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wc.hIconSm = nullptr;

    RegisterClassExA(&wc); /* FIXME: technically you should check the value but, what if you wanted multiple windows? */

    this->hWnd = CreateWindowA("scratch", title.c_str(), WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME), CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, 0, wc.hInstance, nullptr);

    if (this->hWnd == nullptr) {
        Log::logCritical("Failed to initialize Win32: Code " + std::to_string(GetLastError()), true);
        return false;
    }

    hDC = GetDC(this->hWnd);
    this->hDC = CreateCompatibleDC(hDC);
    this->hBitmap = CreateCompatibleBitmap(hDC, width, height);
    SelectObject(this->hDC, this->hBitmap);
    ReleaseDC(this->hWnd, hDC);

    SetWindowLongPtr(this->hWnd, GWLP_USERDATA, (LPARAM)this);

    this->resize(width, height);

    ShowWindow(this->hWnd, SW_NORMAL);
    UpdateWindow(this->hWnd);

    return true;
}

void WindowWin32::cleanup() {
    DeleteObject(this->hBitmap);
    DeleteDC(this->hDC);
    DestroyWindow(this->hWnd);
    /* we dont unregister class here because it would destroy eveyrthing if there are multiple windows */
}

bool WindowWin32::shouldClose() {
    return this->shouldCloseFlag;
}

void WindowWin32::pollEvents() {
    MSG msg;
    while (PeekMessage(&msg, this->hWnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void WindowWin32::swapBuffers() {
    InvalidateRect(this->hWnd, nullptr, FALSE);
    this->pollEvents(); /* this is to pump WM_PAINT */
}

void WindowWin32::resize(int width, int height) {
    RECT rc;

    this->width = width;
    this->height = height;

    rc.left = 0;
    rc.top = 0;
    rc.right = width;
    rc.bottom = height;
    AdjustWindowRect(&rc, GetWindowLongPtr(this->hWnd, GWL_STYLE), FALSE);
    SetWindowPos(this->hWnd, nullptr, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);

    Render::setRenderScale();
    Render::resizeSVGs();
}

int WindowWin32::getWidth() const {
    return this->width;
}

int WindowWin32::getHeight() const {
    return this->height;
}

float WindowWin32::getPixelDensity() const {
    return 1.0;
}

void *WindowWin32::getHandle() {
    return this->hWnd;
}
