#pragma once
#if defined(__APPLE__)
#define SDL_VIDEO_DRIVER_COCOA 1
#endif
#include <SDL.h>
#include <window.hpp>

// 'Window' is a typedef in <X11/Xlib.h>: use 'WindowSE' as class name to avoid conflicts!

class WindowSDL1 : public WindowSE {
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
    SDL_Surface *window = nullptr;
    int width = 0;
    int height = 0;
    bool shouldCloseFlag = false;
};
