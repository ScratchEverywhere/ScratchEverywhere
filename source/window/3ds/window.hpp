#pragma once
#include <window.hpp>
#include <3ds.h>

class Window3DS : public Window {
  public:
    bool init(int width, int height, const std::string &title) override;
    void cleanup() override;

    bool shouldClose() override;
    void pollEvents() override;
    void swapBuffers() override;
    void resize(int width, int height) override;

    int getWidth() const override;
    int getHeight() const override;
    void *getHandle() override;

  private:
    int width = 400;
    int height = 240;
};
