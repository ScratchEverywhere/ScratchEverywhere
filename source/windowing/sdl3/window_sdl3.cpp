#if defined(RENDERER_OPENGL_CORE)
#include <renderers/opengl_core/renderer_gl_core.hpp>
#endif
#ifdef RENDERER_OPENGL
#include <renderers/opengl/renderer_gl.hpp>
#endif
#include "input_sdl3.hpp"
#include "window_sdl3.hpp"
#include <SDL3/SDL_video.h>
#if (defined(_WIN32) || defined(_WIN64) || defined(__APPLE__)) && !defined(__XBOX__)
#include <libdlgmod/libdlgmod.h>
#endif
#include <algorithm>
#include <vector>
#include <cstdlib>
#include <input.hpp>
#include <log.hpp>
#include <math.hpp>
#include <renderer.hpp>
#if defined(RENDERER_SDL3)
#include <renderers/sdl3/renderer_sdl3.hpp>
#endif

#ifdef PLATFORM_HAS_CONTROLLER
SDL_Gamepad *controller = nullptr;
#ifdef __XBOX__
static std::vector<SDL_Gamepad *> xboxControllers;

static void openXboxController(SDL_JoystickID id) {
    for (auto *pad : xboxControllers) {
        if (SDL_GetGamepadID(pad) == id) return;
    }
    if (auto *pad = SDL_OpenGamepad(id)) {
        xboxControllers.push_back(pad);
        if (!controller) controller = pad;
    }
}

static void selectXboxController(SDL_JoystickID id) {
    for (auto *pad : xboxControllers) {
        if (SDL_GetGamepadID(pad) == id && controller != pad) {
            controller = pad;
            Log::log("Xbox active controller: " + std::to_string(id));
            return;
        }
    }
}
#endif
#endif

#ifdef PLATFORM_HAS_TOUCH
bool touchActive = false;
SDL_Point touchPosition;
#endif

#ifdef __XBOX__
#include <hal/xbox.h>
#include <os.hpp>
#if defined(RENDERER_XBOX)
#include <renderers/xbox/renderer_xbox.hpp>
#endif
#endif

WindowSDL3::WindowSDL3(int width, int height, const std::string &title) {
    Input::setGlobalInputDevice(new InputDevice_SDL3);
#if defined(VITA)
    setenv("VITA_DISABLE_TOUCH_BACK", "1", 1);
#endif

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
        error = "Failed to initialize SDL3: " + std::string(SDL_GetError());
        return;
    }

    if (Render::currentRenderer == Renderers::OpenGL) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    } else if (Render::currentRenderer == Renderers::OpenGLCore) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    }

#if defined(__XBOX__)
    SDL_WindowFlags flags = SDL_WINDOW_FULLSCREEN;
#else
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#endif

    if (Render::currentRenderer == Renderers::OpenGL || Render::currentRenderer == Renderers::OpenGLCore) {
        flags |= SDL_WINDOW_OPENGL;
    }

#if defined(__XBOX__)
    if (!SDL_CreateWindowAndRenderer(title.c_str(), width, height, flags, &window, &sdlRenderer)) {
        error = "Failed to create SDL3 window/renderer: " + std::string(SDL_GetError());
        return;
    }
#if defined(RENDERER_XBOX)
    Renderer_Xbox::applyVideoSettings(sdlRenderer);
#endif
#else
    window = SDL_CreateWindow(title.c_str(), width, height, flags);
    if (!window) {
        error = "Failed to create SDL3 window: " + std::string(SDL_GetError());
        return;
    }
#endif

    if (Render::currentRenderer == Renderers::OpenGL || Render::currentRenderer == Renderers::OpenGLCore) {
        context = SDL_GL_CreateContext(window);
        if (!context) {
            error = "Failed to create OpenGL context: " + std::string(SDL_GetError());
            return;
        }

        if (!SDL_GL_MakeCurrent(window, context)) {
            error = "Failed to make OpenGL context current: " + std::string(SDL_GetError());
            return;
        }

        SDL_GL_SetSwapInterval(1); // VSync

#ifdef RENDERER_OPENGL_CORE
        if (!gladLoaderLoadGL()) {
            error = "Failed to initialize GLAD";
            return;
        }

#endif
    }

#ifdef PLATFORM_HAS_CONTROLLER
    int numGamepads;
    SDL_JoystickID *gamepads = SDL_GetGamepads(&numGamepads);
#ifdef __XBOX__
    for (int i = 0; i < numGamepads; ++i) openXboxController(gamepads[i]);
#else
    if (numGamepads > 0) {
        controller = SDL_OpenGamepad(gamepads[0]);
    }
#endif
    SDL_free(gamepads);
#endif

    this->width = width;
    this->height = height;
    this->pixelDensity = SDL_GetWindowPixelDensity(window);

    int dw, dh;
    SDL_GetWindowSizeInPixels(window, &dw, &dh);
    resize(dw, dh);

#if (defined(_WIN32) || defined(_WIN64)) && !defined(__XBOX__)
    widget_set_owner(std::to_string((unsigned long long)(void *)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr)).c_str());
#elif defined(__APPLE__)
    widget_set_owner(std::to_string((unsigned long long)(void *)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr)).c_str());
#endif

    return;
}

void WindowSDL3::cleanup() {
#ifdef PLATFORM_HAS_CONTROLLER
#ifdef __XBOX__
    for (auto *pad : xboxControllers) SDL_CloseGamepad(pad);
    xboxControllers.clear();
    controller = nullptr;
#else
    if (controller) SDL_CloseGamepad(controller);
#endif
#endif
    if (Render::currentRenderer == Renderers::OpenGL || Render::currentRenderer == Renderers::OpenGLCore)
        SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
}

bool WindowSDL3::shouldClose() {
    return shouldCloseFlag;
}

void WindowSDL3::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            OS::toExit = true;
            shouldCloseFlag = true;
            break;
        case SDL_EVENT_WINDOW_RESIZED: {
            int w, h;
            SDL_GetWindowSizeInPixels(window, &w, &h);
            resize(w, h);
        } break;
#ifdef PLATFORM_HAS_CONTROLLER
        case SDL_EVENT_GAMEPAD_ADDED:
#ifdef __XBOX__
            openXboxController(event.gdevice.which);
#else
            if (!controller) controller = SDL_OpenGamepad(event.gdevice.which);
#endif
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
#ifdef __XBOX__
            for (auto it = xboxControllers.begin(); it != xboxControllers.end(); ++it) {
                if (SDL_GetGamepadID(*it) != event.gdevice.which) continue;
                if (controller == *it) controller = nullptr;
                SDL_CloseGamepad(*it);
                xboxControllers.erase(it);
                break;
            }
            if (!controller && !xboxControllers.empty()) controller = xboxControllers.front();
#else
            if (controller && event.gdevice.which == SDL_GetGamepadID(controller)) {
                SDL_CloseGamepad(controller);
                controller = nullptr;
            }
#endif
            break;
#ifdef __XBOX__
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            selectXboxController(event.gbutton.which);
            break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            // Ignore neutral reports and stick drift from other connected pads.
            const bool trigger = event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
            const int threshold = trigger ? 24575 :
                ((event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY || event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTY) ? 18000 : 10000);
            if ((trigger && event.gaxis.value > threshold) ||
                (!trigger && (event.gaxis.value > threshold || event.gaxis.value < -threshold))) {
                selectXboxController(event.gaxis.which);
            }
            break;
        }
#endif
#endif
#ifdef PLATFORM_HAS_TOUCH
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
#endif
#if defined(__PC__)
        case SDL_EVENT_MOUSE_WHEEL:
            Input::mouseScrollDelta += static_cast<int>(event.wheel.y);
            break;
#endif
        }
    }
#if defined(__XBOX__) && defined(PLATFORM_HAS_CONTROLLER)
    // Handle the reset chord outside project key mappings, in menus and games.
    // A hold prevents the Start/Back buttons from triggering an accidental reset.
    static bool resetHeld = false;
    static Uint64 resetHeldSince = 0;
    const bool resetChord = controller &&
        SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_START) &&
        SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_BACK) &&
        SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 24575 &&
        SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 24575;
    if (!resetChord) {
        resetHeld = false;
    } else if (!resetHeld) {
        resetHeld = true;
        resetHeldSince = SDL_GetTicks();
    } else if (SDL_GetTicks() - resetHeldSince >= 750) {
        Log::log("Xbox reset combo: rebooting to the configured dashboard.");
        XReboot();
    }
#endif
}

void WindowSDL3::swapBuffers() {
    if (Render::currentRenderer == Renderers::OpenGL || Render::currentRenderer == Renderers::OpenGLCore)
        SDL_GL_SwapWindow(window);
}

void WindowSDL3::resize(int width, int height) {
    this->width = width;
    this->height = height;
    this->pixelDensity = SDL_GetWindowPixelDensity(window);
#if defined(RENDERER_OPENGL) || defined(RENDERER_OPENGL_CORE)
    if (Render::currentRenderer == Renderers::OpenGL || Render::currentRenderer == Renderers::OpenGLCore)
        glViewport(0, 0, width, height);
#endif
#if defined(__XBOX__) && defined(RENDERER_XBOX)
    if (sdlRenderer) {
        Renderer_Xbox::applyVideoSettings(sdlRenderer);
    }
#endif
    Render::setRenderScale();
    Render::resizeSVGs();
}

int WindowSDL3::getWidth() const {
    return width;
}

int WindowSDL3::getHeight() const {
    return height;
}

float WindowSDL3::getPixelDensity() const {
    return pixelDensity;
}

void *WindowSDL3::getHandle() {
    return window;
}

#if defined(__XBOX__)
void *WindowSDL3::getRenderer() {
    return sdlRenderer;
}
#endif
