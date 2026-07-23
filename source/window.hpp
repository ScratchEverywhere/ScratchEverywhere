#pragma once
#include <string>

// 'Window' is a typedef in <X11/Xlib.h>: use 'WindowSE' as class name to avoid conflicts!

class WindowSE {
  public:
    virtual ~WindowSE() = default;

    virtual bool init(int width, int height, const std::string &title) = 0;
    virtual void cleanup() = 0;

    virtual bool shouldClose() = 0;
    virtual void pollEvents() = 0;
    virtual void swapBuffers() = 0;
    virtual void resize(int width, int height) = 0;

    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    virtual float getPixelDensity() const = 0;
    virtual void *getHandle() = 0;
};

extern WindowSE *globalWindow;
