#pragma once
#include <string>

class Window {
  public:
    virtual ~Window() = default;

    virtual bool init(int width, int height, const std::string &title) = 0;
    virtual void cleanup() = 0;

    virtual bool shouldClose() = 0;
    virtual void pollEvents() = 0;
    virtual void swapBuffers() = 0;
    virtual void resize(int width, int height) = 0;

    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    virtual void *getHandle() = 0;
};

extern Window *globalWindow;
