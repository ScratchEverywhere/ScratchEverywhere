#include "window_win32.hpp"
#include <input.hpp>
#include <log.hpp>
#include <math.hpp>
#include <render.hpp>

#ifdef RENDERER_OPENGL
#include <renderers/opengl/render_opengl.hpp>

#define SWAPBUFFERS
#elif defined(RENDERER_OPENGL_CORE)
#include <renderers/opengl_core/render_opengl_core.hpp>

#define SWAPBUFFERS
#endif

#include "../../../gfx/windows/resource.h"

/* this beautiful code is not unicode safe but i am not going to care i am so sorry */

typedef HRESULT(WINAPI *PFNDWMSETWINDOWATTRIBUTE)(HWND hWnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);

static PFNDWMSETWINDOWATTRIBUTE _DwmSetWindowAttribute = nullptr;
static HMODULE dwmapi = nullptr;

static bool isDarkTheme() {
    DWORD dw;
    DWORD sz = sizeof(dw);
    int err;
    HKEY hkey;
    DWORD type;

    err = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_QUERY_VALUE, &hkey);
    if (err != ERROR_SUCCESS) {
        return false;
    }

    err = RegQueryValueEx(hkey, "AppsUseLightTheme", NULL, &type, (PBYTE)&dw, &sz);
    RegCloseKey(hkey);
    if (err != ERROR_SUCCESS || type != REG_DWORD) {
        return false;
    }

    return !dw;
}

static void setDarkTheme(HWND hWnd, bool dark) {
    BOOL v = dark ? TRUE : FALSE;

    if (_DwmSetWindowAttribute) _DwmSetWindowAttribute(hWnd, 20, &v, sizeof(v));
}

static LRESULT CALLBACK wndproc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    WindowWin32 *self = (WindowWin32 *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (self == nullptr) return DefWindowProc(hWnd, msg, wp, lp);

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hDC = BeginPaint(hWnd, &ps);
        RECT r;

        GetClientRect(hWnd, &r);

#ifndef SWAPBUFFERS
        BitBlt(hDC, 0, 0, self->getWidth(), self->getHeight(), self->hDC, 0, 0, SRCCOPY);
#endif
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
    case WM_WININICHANGE: {
        char *s = (char *)lp;
        if (s != NULL && strcmp(s, "ImmersiveColorSet") == 0) setDarkTheme(hWnd, isDarkTheme());
        break;
    }
    case WM_SIZE: {
        self->resize(LOWORD(lp), HIWORD(lp));
        break;
    }
    default: {
        return DefWindowProc(hWnd, msg, wp, lp);
    }
    }
    return 0;
}

#if defined(RENDERER_OPENGL) || defined(RENDERER_OPENGL_CORE)
typedef BOOL(APIENTRY *PFNWGLSWAPINTERVALEXT)(int);
#endif

bool WindowWin32::init(int width, int height, const std::string &title) {
    WNDCLASSEX wc;
#if defined(RENDERER_OPENGL) || defined(RENDERER_OPENGL_CORE)
    PIXELFORMATDESCRIPTOR pfd;
    int pf;
    PFNWGLSWAPINTERVALEXT swap_interval_ext = NULL;
#endif

    this->shouldCloseFlag = 0;

    if (dwmapi == nullptr && _DwmSetWindowAttribute == nullptr) {
        dwmapi = LoadLibraryA("dwmapi.dll");

        _DwmSetWindowAttribute = (PFNDWMSETWINDOWATTRIBUTE)GetProcAddress(dwmapi, "DwmSetWindowAttribute");
    }

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

    this->hWnd = CreateWindowA("scratch", title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, 0, wc.hInstance, nullptr);

    if (this->hWnd == nullptr) {
        Log::logCritical("Failed to initialize Win32: Code " + std::to_string(GetLastError()), true);
        return false;
    }

    setDarkTheme(this->hWnd, isDarkTheme());

#ifdef SWAPBUFFERS
    this->winDC = GetDC(this->hWnd);
    this->hDC = this->winDC;
#else
    this->winDC = GetDC(this->hWnd);
    this->hDC = CreateCompatibleDC(this->winDC);
    this->hBitmap = CreateCompatibleBitmap(this->winDC, width, height);
    SelectObject(this->hDC, this->hBitmap);
#endif

#if defined(RENDERER_OPENGL) || defined(RENDERER_OPENGL_CORE)
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.cDepthBits = 32;
    pfd.cColorBits = 32;

    pf = ChoosePixelFormat(this->hDC, &pfd);
    SetPixelFormat(this->hDC, pf, &pfd);

    this->hGLRC = wglCreateContext(this->hDC);
    wglMakeCurrent(this->hDC, this->hGLRC);

    if ((swap_interval_ext =
             (PFNWGLSWAPINTERVALEXT)wglGetProcAddress("wglSwapIntervalEXT")) != NULL) {
        swap_interval_ext(1);
    }

#ifdef RENDERER_OPENGL_CORE
    if (!gladLoaderLoadGL()) {
        Log::logCritical("Failed to initialize GLAD", true);
        return false;
    }
#endif
#endif

    SetWindowLongPtr(this->hWnd, GWLP_USERDATA, (LPARAM)this);

    this->resize(width, height);

    ShowWindow(this->hWnd, SW_NORMAL);
    UpdateWindow(this->hWnd);

    return true;
}

void WindowWin32::cleanup() {
#if defined(RENDERER_OPENGL) || defined(RENDERER_OPENGL_CORE)
    wglMakeCurrent(NULL, NULL);
#endif

#ifndef SWAPBUFFERS
    DeleteObject(this->hBitmap);
    DeleteDC(this->hDC);
#endif
    ReleaseDC(this->hWnd, this->winDC);

#if defined(RENDERER_OPENGL) || defined(RENDERER_OPENGL_CORE)
    wglDeleteContext(this->hGLRC);
#endif

    DestroyWindow(this->hWnd);
    /* we dont unregister class here because it would destroy eveyrthing if there are multiple windows */
    /* we also dont free library for same reason */
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
#ifdef SWAPBUFFERS
    SwapBuffers(this->hDC);
#else
    InvalidateRect(this->hWnd, nullptr, FALSE);
    this->pollEvents(); /* this is to pump WM_PAINT */
#endif
}

void WindowWin32::resize(int width, int height) {
    RECT rc;
    RECT wrc;

    this->width = width;
    this->height = height;

    rc.left = 0;
    rc.top = 0;
    rc.right = width;
    rc.bottom = height;
    AdjustWindowRect(&rc, GetWindowLongPtr(this->hWnd, GWL_STYLE), FALSE);

#ifndef SWAPBUFFERS
    DeleteObject(this->hBitmap);
    this->hBitmap = CreateCompatibleBitmap(this->winDC, width, height);
    SelectObject(this->hDC, this->hBitmap);

    this->resized(width, height);
#endif

    GetWindowRect(this->hWnd, &wrc);
    if ((wrc.right - wrc.left) == (rc.right - rc.left) && (wrc.bottom - wrc.top) == (rc.bottom - rc.top)) return;

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
