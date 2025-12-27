#include "window.hpp"
#include "../render.hpp"
#include <input.hpp>
#include <math.hpp>
#include <render.hpp>

SDL_Gamepad *controller = nullptr;
bool touchActive = false;
SDL_Point touchPosition;

bool Sdl3Window::init(int w, int h, const std::string &title) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
        Log::logError("Failed to initialize SDL3: " + std::string(SDL_GetError()));
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    window = SDL_CreateWindow(title.c_str(), w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        Log::logError("Failed to create SDL3 window: " + std::string(SDL_GetError()));
        return false;
    }

    context = SDL_GL_CreateContext(window);
    if (!context) {
        Log::logError("Failed to create OpenGL context: " + std::string(SDL_GetError()));
        return false;
    }

    SDL_GL_SetSwapInterval(1); // VSync

    int numGamepads;
    SDL_JoystickID *gamepads = SDL_GetGamepads(&numGamepads);
    if (numGamepads > 0) {
        controller = SDL_OpenGamepad(gamepads[0]);
    }
    SDL_free(gamepads);

    this->width = w;
    this->height = h;

    int dw, dh;
    SDL_GetWindowSizeInPixels(window, &dw, &dh);
    resize(dw, dh);

    return true;
}

void Sdl3Window::cleanup() {
    if (controller) SDL_CloseGamepad(controller);
    if (context) SDL_GL_DestroyContext(context);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

bool Sdl3Window::shouldClose() {
    return shouldCloseFlag;
}

void Sdl3Window::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            shouldCloseFlag = true;
            break;
        case SDL_EVENT_WINDOW_RESIZED: {
            int w, h;
            SDL_GetWindowSizeInPixels(window, &w, &h);
            resize(w, h);
        } break;
        case SDL_EVENT_GAMEPAD_ADDED:
            if (!controller) controller = SDL_OpenGamepad(event.gdevice.which);
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (controller && event.gdevice.which == SDL_GetGamepadID(controller)) {
                SDL_CloseGamepad(controller);
                controller = nullptr;
            }
            break;
        case SDL_EVENT_FINGER_DOWN:
            touchActive = true;
            touchPosition = {
                static_cast<int>(event.tfinger.x * width),
                static_cast<int>(event.tfinger.y * height)};
            break;
        case SDL_EVENT_FINGER_MOTION:
            touchPosition = {
                static_cast<int>(event.tfinger.x * width),
                static_cast<int>(event.tfinger.y * height)};
            break;
        case SDL_EVENT_FINGER_UP:
            touchActive = false;
            break;
        }
    }
}

void Sdl3Window::swapBuffers() {
    SDL_GL_SwapWindow(window);
}

void Sdl3Window::resize(int width, int height) {
    this->width = width;
    this->height = height;
    glViewport(0, 0, width, height);
    Render::setRenderScale();
}

int Sdl3Window::getWidth() const {
    return width;
}

int Sdl3Window::getHeight() const {
    return height;
}

void *Sdl3Window::getHandle() {
    return window;
}
