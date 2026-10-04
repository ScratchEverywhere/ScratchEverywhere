#include "window_win32.hpp"
#include <input.hpp>
#include <log.hpp>
#include <math.hpp>
#include <render.hpp>

/* this beautiful code is not unicode safe but i am not going to care i am so sorry */

static LRESULT CALLBACK wndproc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    return DefWindowProc(hWnd, msg, wp, lp);
}

bool WindowWin32::init(int width, int height, const std::string &title) {
    WNDCLASSEX wc;

    this->shouldCloseFlag = 0;

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndproc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = (HINSTANCE)GetModuleHandle(nullptr);
    wc.lpszClassName = "se";
    wc.lpszMenuName = nullptr;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_MENU);
    wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);
    wc.hIconSm = nullptr;

    RegisterClassExA(&wc); /* FIXME: technically you should check the value but, what if you wanted multiple windows? */

    this->hWnd = CreateWindowA("se", title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, 0, wc.hInstance, nullptr);

    if (this->hWnd == nullptr) {
        Log::logCritical("Failed to initialize Win32: Code " + std::to_string(GetLastError()), true);
        return false;
    }

    SetWindowLongPtr(this->hWnd, GWLP_USERDATA, (LPARAM)this);

    ShowWindow(this->hWnd, SW_NORMAL);
    UpdateWindow(this->hWnd);

    this->resize(width, height);

    return true;
}

void WindowWin32::cleanup() {
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
}

void WindowWin32::resize(int width, int height) {
    this->width = width;
    this->height = height;

    SetWindowPos(this->hWnd, nullptr, 0, 0, width, height, SWP_NOZORDER | SWP_NOMOVE);

    Render::setRenderScale();
    Render::resizeSVGs();
}

int WindowWin32::getWidth() const {
    return width;
}

int WindowWin32::getHeight() const {
    return height;
}

float WindowWin32::getPixelDensity() const {
    return 1;
}

void *WindowWin32::getHandle() {
    return this->hWnd;
}
