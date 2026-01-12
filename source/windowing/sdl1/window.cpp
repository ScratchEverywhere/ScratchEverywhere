#include "window.hpp"
#include <chrono>
#include <input.hpp>
#include <math.hpp>
#include <render.hpp>
#include <thread>
#ifdef RENDERER_OPENGL
#include <renderers/opengl/render.hpp>
#else
#include <renderers/sdl1/render.hpp>
#endif

SDL_Joystick *controller = nullptr;

#ifdef __PC__
bool isFullscreen = false;
#endif

#ifdef RENDERER_OPENGL
static auto lastFrameTime = std::chrono::high_resolution_clock::now();
static const int TARGET_FPS = 60; // SDL1 OpenGL target frame rate for VSync-like behavior
#endif

bool WindowSDL1::init(int width, int height, const std::string &title) {
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

    if (SDL_NumJoysticks() > 0) controller = SDL_JoystickOpen(0);

    this->width = width;
    this->height = height;

    resize(width, height);

    // Print SDL version number. could be useful for debugging
    SDL_version ver;
    SDL_VERSION(&ver);
    Log::log("SDL v" + std::to_string(ver.major) + "." + std::to_string(ver.minor) + "." + std::to_string(ver.patch));

    return true;
}

#ifdef __PC__
void WindowSDL1::toggleFullscreen()
{
    isFullscreen = !isFullscreen;

    Uint32 flags = SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_RESIZABLE;
#ifdef RENDERER_OPENGL
    flags |= SDL_OPENGL;
#endif

    if (isFullscreen) {
        flags |= SDL_FULLSCREEN;
    }

    window = SDL_SetVideoMode(width, height, 32, flags);

    if (!window) {
        Log::logError("<SDL> Cannot toggle fullscreen.");
        return;
    }

#ifdef RENDERER_OPENGL
    glViewport(0, 0, width, height);
#endif

    resize(width, height);
}
#endif

void WindowSDL1::cleanup() {
    if (controller) SDL_JoystickClose(controller);
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
#ifdef __PC__
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_F11) {
                toggleFullscreen();
            }
            break;
#endif
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
}

int WindowSDL1::getWidth() const {
    return width;
}

int WindowSDL1::getHeight() const {
    return height;
}

void *WindowSDL1::getHandle() {
    return window;
}
