#include "window.hpp"
#include <input.hpp>
#include <log.hpp>
#include <math.hpp>
#include <render.hpp>
#ifdef RENDERER_OPENGL
#include <renderers/opengl/render.hpp>
#else
#include <renderers/sdl2/render.hpp>
#endif

#ifdef __PS4__
#include <orbis/Net.h>
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h>
#endif

#ifdef PLATFORM_HAS_CONTROLLER
SDL_GameController *controller = nullptr;
#endif

#ifdef PLATFORM_HAS_TOUCH
bool touchActive = false;
SDL_Point touchPosition;
#endif

bool WindowSDL2::init(int width, int height, const std::string &title) {
#if defined(VITA)
    SDL_setenv("VITA_DISABLE_TOUCH_BACK", "1", 1);
#endif

    uint32_t sdlFlags = 0;
// SDL has to be initialized before window creation on webOS
#ifndef WEBOS
    sdlFlags = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
#ifdef PLATFORM_HAS_CONTROLLER
    sdlFlags |= SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER;
#endif
    if (SDL_Init(sdlFlags) < 0) {
        Log::logError("Failed to initialize SDL2: " + std::string(SDL_GetError()));
        return false;
    }
#endif

#ifdef RENDERER_OPENGL
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
#elif defined(__PS4__)
    SDL_GL_SetSwapInterval(1); // Required for VSync
#endif

#ifdef WEBOS
    Uint32 flags = SDL_WINDOW_FULLSCREEN | SDL_WINDOW_ALLOW_HIGHDPI;
#else
    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#endif

#ifdef RENDERER_OPENGL
    flags |= SDL_WINDOW_OPENGL;
#endif

    window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);
    if (!window) {
        Log::logError("Failed to create SDL2 window: " + std::string(SDL_GetError()));
        return false;
    }

#ifdef RENDERER_OPENGL
    context = SDL_GL_CreateContext(window);
    if (!context) {
        Log::logError("Failed to create OpenGL context: " + std::string(SDL_GetError()));
        return false;
    }

    SDL_GL_SetSwapInterval(1); // VSync
#endif

#ifdef PLATFORM_HAS_CONTROLLER
    if (SDL_NumJoysticks() > 0) controller = SDL_GameControllerOpen(0);
#endif

    this->width = width;
    this->height = height;

#ifdef RENDERER_OPENGL
    int dw, dh;
    SDL_GL_GetDrawableSize(window, &dw, &dh);
    resize(dw, dh);
#else
    int dw, dh;
#ifdef __PS4__
    SDL_GetWindowSize(window, &dw, &dh);
#else
    SDL_GetWindowSizeInPixels(window, &dw, &dh);
#endif
    resize(dw, dh);
#endif

    // Print SDL version number. could be useful for debugging
    SDL_version ver;
    SDL_VERSION(&ver);
    Log::log("SDL v" + std::to_string(ver.major) + "." + std::to_string(ver.minor) + "." + std::to_string(ver.patch));

    return true;
}

void WindowSDL2::cleanup() {
#ifdef PLATFORM_HAS_CONTROLLER
    if (controller) SDL_GameControllerClose(controller);
#endif

#ifdef RENDERER_OPENGL
    SDL_GL_DeleteContext(context);
#endif
    SDL_DestroyWindow(window);
}

bool WindowSDL2::shouldClose() {
    return shouldCloseFlag;
}

void WindowSDL2::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            OS::toExit = true;
            shouldCloseFlag = true;
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                int w, h;
#ifdef RENDERER_OPENGL
                SDL_GL_GetDrawableSize(window, &w, &h);
#elif defined(__PS4__)
                SDL_GetWindowSize(window, &w, &h);
#else
                SDL_GetWindowSizeInPixels(window, &w, &h);
#endif
                resize(w, h);
            }
            break;
#ifdef PLATFORM_HAS_CONTROLLER
        case SDL_CONTROLLERDEVICEADDED:
            if (!controller) controller = SDL_GameControllerOpen(event.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (controller && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))) {
                SDL_GameControllerClose(controller);
                controller = nullptr;
            }
            break;
#endif
#ifdef PLATFORM_HAS_TOUCH
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
#endif
        }
    }
}

void WindowSDL2::swapBuffers() {
#ifdef RENDERER_OPENGL
    SDL_GL_SwapWindow(window);
#endif
}

void WindowSDL2::resize(int width, int height) {
    this->width = width;
    this->height = height;
#ifdef RENDERER_OPENGL
    glViewport(0, 0, width, height);
#endif
    Render::setRenderScale();
    Render::resizeSVGs();
}

int WindowSDL2::getWidth() const {
    return width;
}

int WindowSDL2::getHeight() const {
    return height;
}

void *WindowSDL2::getHandle() {
    return window;
}
