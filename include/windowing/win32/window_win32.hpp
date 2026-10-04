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

  private:
    HWND hWnd;

    int width;
    int height;

    int shouldCloseFlag;
};
