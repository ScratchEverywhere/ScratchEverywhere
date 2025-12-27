#pragma once
#include "../window.hpp"
#define GL_INCLUDE_NONE
#include <GLFW/glfw3.h>

class WindowGLFW : public Window {
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
    GLFWwindow *window = nullptr;
    int width = 0;
    int height = 0;
};
