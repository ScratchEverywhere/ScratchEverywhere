#pragma once
#include <window.hpp>
#include <SDL3/SDL.h>

class WindowSDL3 : public Window {
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
    SDL_Window *window = nullptr;
    SDL_GLContext context = nullptr;
    int width = 0;
    int height = 0;
    bool shouldCloseFlag = false;
};
