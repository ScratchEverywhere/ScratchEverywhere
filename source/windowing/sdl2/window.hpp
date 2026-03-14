#pragma once
#include <SDL2/SDL.h>
#include <window.hpp>

class WindowSDL2 : public Window {
  public:
    bool init(int width, int height, const std::string &title) override;
    void cleanup() override;

    bool shouldClose() override;
    void pollEvents() override;
    void swapBuffers() override;
    void toggleFullscreen();
    void resize(int width, int height) override;

    int getWidth() const override;
    int getHeight() const override;
    void *getHandle() override;

  private:
    SDL_Window *window = nullptr;
    SDL_GLContext context = nullptr;
    int width = 0;
    int height = 0;
    bool shouldCloseFlag = false;
};
