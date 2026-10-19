#pragma once
#include <gl2d.h>
#include <nds.h>
#include <se_export.hpp>
#include <window.hpp>

class SE_EXPORT WindowNDS : public WindowSE {
  public:
    WindowNDS(int width, int height, const std::string &title);
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
    int width = 256;
    int height = 192;
};
