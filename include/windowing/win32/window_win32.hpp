#pragma once
#define WIN32_MEAN_AND_LEAN
#include <se_export.hpp>
#include <window.hpp>
#include <windows.h>
#ifdef RENDERER_D3D9
#include <d3d9.h>
#endif

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

    bool resized;

    /* these are here because, well, win32 api. */
    int shouldCloseFlag;
    HDC hDC;
    HDC winDC;

#if defined(RENDERER_D3D9)
    D3DPRESENT_PARAMETERS dpp;
    IDirect3D9 *pD3D;
    IDirect3DDevice9 *pDev;
#endif

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
