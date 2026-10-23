#pragma once
#include <SDL.h>
#include <se_export.hpp>
#include <window.hpp>

class SE_EXPORT WindowSDL1 : public WindowSE {
  public:
    WindowSDL1(int width, int height, const std::string &title);
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
    SDL_Surface *window = nullptr;
    int width = 0;
    int height = 0;
    bool shouldCloseFlag = false;
};
