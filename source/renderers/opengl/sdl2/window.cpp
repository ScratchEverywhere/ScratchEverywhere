#include "window.hpp"
#include <input.hpp>
#include <math.hpp>
#include <render.hpp>
#include "../render.hpp"
#include <iostream>

SDL_GameController *controller = nullptr;
bool touchActive = false;
SDL_Point touchPosition;

bool Sdl2Window::init(int w, int h, const std::string &title) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) < 0) {
        Log::logError("Failed to initialize SDL2");
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        Log::logError("Failed to create SDL2 window");
        return false;
    }

    context = SDL_GL_CreateContext(window);
    if (!context) {
        Log::logError("Failed to create OpenGL context");
        return false;
    }

    SDL_GL_SetSwapInterval(1); // VSync

    if (SDL_NumJoysticks() > 0) controller = SDL_GameControllerOpen(0);

    this->width = w;
    this->height = h;

    int dw, dh;
    SDL_GL_GetDrawableSize(window, &dw, &dh);
    resize(dw, dh);

    // Print SDL version number. could be useful for debugging
    SDL_version ver;
    SDL_VERSION(&ver);
    Log::log("SDL v" + std::to_string(ver.major) + "." + std::to_string(ver.minor) + "." + std::to_string(ver.patch));

    return true;
}

void Sdl2Window::cleanup() {
    if (controller) SDL_GameControllerClose(controller);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

bool Sdl2Window::shouldClose() {
    return shouldCloseFlag;
}

void Sdl2Window::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            shouldCloseFlag = true;
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                int w, h;
                SDL_GL_GetDrawableSize(window, &w, &h);
                resize(w, h);
            }
            break;
        case SDL_CONTROLLERDEVICEADDED:
            if (!controller) controller = SDL_GameControllerOpen(event.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (controller && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))) {
                SDL_GameControllerClose(controller);
                controller = nullptr;
            }
            break;
        case SDL_FINGERDOWN:
            touchActive = true;
            touchPosition = {
                static_cast<int>(event.tfinger.x * width),
                static_cast<int>(event.tfinger.y * height)};
            break;
        case SDL_FINGERMOTION:
            touchPosition = {
                static_cast<int>(event.tfinger.x * width),
                static_cast<int>(event.tfinger.y * height)};
            break;
        case SDL_FINGERUP:
            touchActive = false;
            break;
        }
    }
}

void Sdl2Window::swapBuffers() {
    SDL_GL_SwapWindow(window);
}

void Sdl2Window::resize(int width, int height) {
    this->width = width;
    this->height = height;
    glViewport(0, 0, width, height);
    Render::setRenderScale();
}

int Sdl2Window::getWidth() const {
    return width;
}

int Sdl2Window::getHeight() const {
    return height;
}

void *Sdl2Window::getHandle() {
    return window;
}
