#pragma once
#include <se_export.hpp>
#include <window.hpp>

#ifdef RENDERER_OPENGL_CORE
#include <glad/gl.h>
#endif

#define GL_INCLUDE_NONE
#include <GLFW/glfw3.h>

class SE_EXPORT WindowGLFW : public WindowSE {
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
    GLFWwindow *window = nullptr;
    int width = 0;
    int height = 0;
};
