#pragma once
#include <window.hpp>

// 'Window' is a typedef in <X11/Xlib.h>: use 'WindowSE' as class name to avoid conflicts!

class WindowLibretro : public WindowSE {
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
    int width = 0;
    int height = 0;
};
