#include "window.hpp"
#if defined(_WIN32) || defined(_WIN64) || defined(__APPLE__) || (defined(__linux__) && !defined(__ANDROID__) && !defined(WEBOS)) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || (defined(__sun) && defined(__SVR4))
#include <SDL_syswm.h>
#include <libdlgmod/libdlgmod.h>
#endif
#include <chrono>
#if defined(SDL_VIDEO_DRIVER_X11) && ((defined(__linux__) && !defined(__ANDROID__) && !defined(WEBOS)) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || (defined(__sun) && defined(__SVR4)))
#include <cstdlib>
#endif
#include <input.hpp>
#include <log.hpp>
#include <math.hpp>
#include <render.hpp>
#include <thread>
#ifdef RENDERER_OPENGL
#include <renderers/opengl/render.hpp>
#else
#include <renderers/sdl1/render.hpp>
#endif

#ifdef PLATFORM_HAS_CONTROLLER
SDL_Joystick *controller = nullptr;
#endif

#ifdef RENDERER_OPENGL
static auto lastFrameTime = std::chrono::high_resolution_clock::now();
static const int TARGET_FPS = 60; // SDL1 OpenGL target frame rate for VSync-like behavior
#endif

bool WindowSDL1::init(int width, int height, const std::string &title) {
#if defined(SDL_VIDEO_DRIVER_X11) && ((defined(__linux__) && !defined(__ANDROID__) && !defined(WEBOS)) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || (defined(__sun) && defined(__SVR4)))
    setenv("SDL_VIDEODRIVER", "x11", 1);
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        Log::logError("Failed to initialize SDL1");
        return false;
    }
    SDL_EnableUNICODE(1);
    SDL_WM_SetCaption(title.c_str(), NULL);

#ifdef RENDERER_OPENGL
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    window = SDL_SetVideoMode(width, height, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_RESIZABLE | SDL_OPENGL);
#else
    window = SDL_SetVideoMode(width, height, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_RESIZABLE);
#endif

    if (!window) {
        Log::logError("Failed to create SDL1 window");
        return false;
    }

#ifdef PLATFORM_HAS_CONTROLLER
    if (SDL_NumJoysticks() > 0) controller = SDL_JoystickOpen(0);
#endif

    this->width = width;
    this->height = height;

    resize(width, height);

#if defined(_WIN32) || defined(_WIN64) || defined(__APPLE__) || (defined(__linux__) && !defined(__ANDROID__) && !defined(WEBOS)) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || (defined(__sun) && defined(__SVR4))
	SDL_SysWMinfo system_info;
	SDL_VERSION(&system_info.version);
	SDL_GetWMInfo(&system_info);
#if defined(_WIN32) || defined(_WIN64)
	widget_set_owner(std::to_string((unsigned long long)(void *)system_info.info.win.window).c_str());
#elif defined(__APPLE__)
	widget_set_owner(std::to_string((unsigned long long)(void *)system_info.info.cocoa.window).c_str());
#elif (defined(__linux__) && !defined(__ANDROID__) && !defined(WEBOS)) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || (defined(__sun) && defined(__SVR4))
	widget_set_owner(std::to_string((unsigned long long)(unsigned long)system_info.info.x11.window).c_str());
#endif
#endif

    // Print SDL version number. could be useful for debugging
    SDL_version ver;
    SDL_VERSION(&ver);
    Log::log("SDL v" + std::to_string(ver.major) + "." + std::to_string(ver.minor) + "." + std::to_string(ver.patch));

    return true;
}

void WindowSDL1::cleanup() {
#ifdef PLATFORM_HAS_CONTROLLER
    if (controller) SDL_JoystickClose(controller);
#endif
    SDL_Quit();
}

bool WindowSDL1::shouldClose() {
    return shouldCloseFlag;
}

void WindowSDL1::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            OS::toExit = true;
            shouldCloseFlag = true;
            break;
        case SDL_VIDEORESIZE:
            resize(event.resize.w, event.resize.h);
            break;
        }
    }
}

void WindowSDL1::swapBuffers() {
#ifdef RENDERER_OPENGL
    SDL_GL_SwapBuffers();

    // Frame rate limiting for SDL1 OpenGL (VSync-like behavior)
    // SDL1 doesn't support SDL_GL_SetSwapInterval, so we manually limit frame rate
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - lastFrameTime);
    auto frameTime = std::chrono::microseconds(1000000 / TARGET_FPS);

    if (elapsed < frameTime) {
        std::this_thread::sleep_for(frameTime - elapsed);
    }
    lastFrameTime = std::chrono::high_resolution_clock::now();
#endif
}

void WindowSDL1::resize(int width, int height) {
    this->width = width;
    this->height = height;
#ifdef RENDERER_OPENGL
    window = SDL_SetVideoMode(width, height, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_RESIZABLE | SDL_OPENGL);
    if (window) {
        glViewport(0, 0, width, height);
    }
#else
    window = SDL_SetVideoMode(width, height, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_RESIZABLE);
#endif
    Render::setRenderScale();
    Render::resizeSVGs();
}

int WindowSDL1::getWidth() const {
    return width;
}

int WindowSDL1::getHeight() const {
    return height;
}

float WindowSDL1::getPixelDensity() const {
    return 1.0f;
}

void *WindowSDL1::getHandle() {
    return window;
}
