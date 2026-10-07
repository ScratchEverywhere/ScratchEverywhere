#pragma once
#define WIN32_MEAN_AND_LEAN
#include <se_export.hpp>
#include <window.hpp>
#include <windows.h>

class SE_EXPORT WindowWin32 : public WindowSE {
  public:
    bool init(int width, int height, const std::string &title) override;
    void cleanup() override;

    bool shouldClose() override;
    void pollEvents() override;
    void swapBuffers() override;
    void resize(int width, int height) override;

    int getWidth() const override;
    int getHeight() const override;
    float getPixelDensity() const override;
    void *getHandle() override;

    void resized(int width, int height);

    /* these are here because, well, win32 api. */
    int shouldCloseFlag;
    HDC hDC;
    HDC winDC;

  private:
    HWND hWnd;
#if defined(RENDERER_OPENGL) || defined(RENDERER_OPENGL_CORE)
    HGLRC hGLRC;
#else
    HBITMAP hBitmap;
#endif

    int width;
    int height;
};
