#pragma once
#include <window.hpp>

class WindowHeadless : public Window {
  public:
    bool init(int width, int height, const std::string &title) override { return true; }
    void cleanup() override {}

    bool shouldClose() override { return false; }
    void pollEvents() override {}
    void swapBuffers() override {}
    void resize(int width, int height) override {}

    int getWidth() const override { return 0; }
    int getHeight() const override { return 0; }
    float getPixelDensity() const override { return 1.0f; }
    void *getHandle() override { return nullptr; }
};
