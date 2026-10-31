#pragma once
#include <se_export.hpp>
#include <window.hpp>

class SE_EXPORT WindowWiiU : public WindowSE {
  public:
    WindowWiiU(int width, int height, const std::string &title);
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
    int width = 854;
    int height = 480;
    bool gfxReady = false;
    bool procReady = false;
};
