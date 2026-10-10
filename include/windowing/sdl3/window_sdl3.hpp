#pragma once
#include <SDL3/SDL.h>
#include <se_export.hpp>
#include <window.hpp>

class SE_EXPORT WindowSDL3 : public WindowSE {
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
#if defined(__XBOX__)
    void *getRenderer();
#endif

  private:
#if defined(__XBOX__)
    SDL_Renderer *sdlRenderer = nullptr;
#endif
    SDL_Window *window = nullptr;
    SDL_GLContext context = nullptr;
    int width = 0;
    int height = 0;
    float pixelDensity = 1.0f;
    bool shouldCloseFlag = false;
};
