#pragma once
#include <se_export.hpp>
#include <string>
#include <window.hpp>

class SE_EXPORT WindowXbox : public WindowSE {
  public:
    WindowXbox(int width, int height, const std::string &title);
    ~WindowXbox() override = default;

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
    float pixelDensity = 1.0f;
    bool shouldCloseFlag = false;
};
