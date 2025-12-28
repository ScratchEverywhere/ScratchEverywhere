#pragma once
#include <gl2d.h>
#include <nds.h>
#include <window.hpp>

class WindowNDS : public Window {
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
    int width = 256;
    int height = 192;
};
