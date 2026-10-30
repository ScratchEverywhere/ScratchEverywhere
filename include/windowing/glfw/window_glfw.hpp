#pragma once
#if defined(RENDERER_OPENGL_CORE) && !defined(RENDERER_OPENGL)
#include <glad/gl.h>
#endif

#include <se_export.hpp>
#include <window.hpp>

#define GL_INCLUDE_NONE
#include <GLFW/glfw3.h>

class SE_EXPORT WindowGLFW : public WindowSE {
  public:
    WindowGLFW(int width, int height, const std::string &title);
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
    GLFWwindow *window = nullptr;
    int width = 0;
    int height = 0;
};
