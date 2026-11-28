#include "SDL3/SDL_error.h"
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
#include <cstdlib>
#include <input.hpp>
#include <log.hpp>
#include <math.hpp>
#include <os.hpp>
#include <renderer.hpp>
#include <vector>
#if defined(RENDERER_SDL3)
#include <renderers/sdl3/renderer_sdl3.hpp>
#endif
#include <utf8.hpp>
#ifdef __UBUNTUTOUCH__
#include <platforms/ut/swkbd_ut.hpp>

extern bool utKeyboardShown;
UbuntuTouchKeyboard &getUbuntuTouchKeyboard();
#endif

#ifdef PLATFORM_HAS_CONTROLLER
SDL_Gamepad *controller = nullptr;
#endif

#ifdef PLATFORM_HAS_TOUCH
bool touchActive = false;
SDL_Point touchPosition;
#endif

WindowSDL3::WindowSDL3(int width, int height, const std::string &title) {
    Input::setGlobalInputDevice(new InputDevice_SDL3);
#if defined(VITA)
    setenv("VITA_DISABLE_TOUCH_BACK", "1", 1);
#endif

#if defined(__XBOX__) && defined(SE_XBOX_TRACE_RENDERER)
    SDL_SetLogOutputFunction([](void *, int, SDL_LogPriority, const char *message) {
        Log::log(std::string("SDL: ") + message);
    },
                             nullptr);
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
    for (int i = 0; i < numGamepads; ++i)
        openXboxController(gamepads[i]);
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
    for (auto *pad : xboxControllers)
        SDL_CloseGamepad(pad);
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
    Input::mouseScrollDelta = 0;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            OS::toExit = true;
            shouldCloseFlag = true;
            break;
#ifdef SE_FILE_DROP
        case SDL_EVENT_DROP_FILE:
            if (event.drop.data && OS::pendingOpenFile.empty()) OS::pendingOpenFile = event.drop.data;
            break;
#endif
        case SDL_EVENT_WINDOW_RESIZED: {
            int w, h;
            SDL_GetWindowSizeInPixels(window, &w, &h);
            resize(w, h);
        } break;
#ifdef PLATFORM_HAS_CONTROLLER
        case SDL_EVENT_GAMEPAD_ADDED: {
            if (!controller) controller = SDL_OpenGamepad(event.gdevice.which);

            if (!controller) {
                Log::logError("Failed to connect controller: " + std::string(SDL_GetError()));
                break;
            }

            const char *name = SDL_GetGamepadName(controller);
            if (!name) break;
            Log::log("[SDL3] Controller connected: " + std::string(name));
            if (SDL_strstr(name, "Nintendo")) {
                Input::setControllerType(ControllerType::Nintendo);
            } else if (SDL_strstr(name, "Xbox")) {
                Input::setControllerType(ControllerType::Xbox);
            } else if (SDL_strstr(name, "Playstation") || SDL_strstr(name, "PS")) {
                Input::setControllerType(ControllerType::Playstation);
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (controller && event.gdevice.which == SDL_GetGamepadID(controller)) {
                SDL_CloseGamepad(controller);
                controller = nullptr;
            }
            Input::setControllerType(ControllerType::Unknown);
            break;
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
#ifndef SDL3_NO_TEXT_INPUT
        case SDL_EVENT_TEXT_INPUT:
            Input::inputText += event.text.text;
            break;

        case SDL_EVENT_KEY_DOWN:
            if (!Input::isTextInputActive()) break;
            switch (event.key.scancode) {
            case SDL_SCANCODE_RETURN:
            case SDL_SCANCODE_KP_ENTER:
            case SDL_SCANCODE_ESCAPE:
                Input::endTextInput();
                break;
            case SDL_SCANCODE_BACKSPACE:
                if (!Input::inputText.empty()) {
                    utf8::eraseLast(Input::inputText, 1);
                }
                break;
            case SDL_SCANCODE_V: {
#ifdef __APPLE__
                bool pasteMod = (event.key.mod & SDL_KMOD_GUI) != 0;
#else
                bool pasteMod = (event.key.mod & SDL_KMOD_CTRL) != 0;
#endif
                if (pasteMod && SDL_HasClipboardText()) {
                    char *clipboard = SDL_GetClipboardText();
                    if (clipboard) {
                        std::string pasted(clipboard);
                        SDL_free(clipboard);

                        pasted.erase(std::remove(pasted.begin(), pasted.end(), '\r'), pasted.end());
                        pasted.erase(std::remove(pasted.begin(), pasted.end(), '\n'), pasted.end());

                        Input::inputText += pasted;
                    }
                }
                break;
            }
            default:
                break;
            }
            break;
#endif
        }
    }
#ifdef __UBUNTUTOUCH__
    if (utKeyboardShown) {
        UbuntuTouchKeyboard &maliitKeyboard = getUbuntuTouchKeyboard();
        maliitKeyboard.poll();
        for (const UbuntuTouchKeyboard::Event &oskEvent : maliitKeyboard.takeEvents()) {
            switch (oskEvent.type) {
            case UbuntuTouchKeyboard::Event::Type::CommitString:
                utf8::eraseLast(Input::inputText, static_cast<size_t>(std::max(oskEvent.replaceLength, 0)));
                Input::inputText += oskEvent.text;
                break;

            case UbuntuTouchKeyboard::Event::Type::Backspace:
                utf8::eraseLast(Input::inputText, 1);
                break;

            case UbuntuTouchKeyboard::Event::Type::Enter:
            case UbuntuTouchKeyboard::Event::Type::Escape:
            case UbuntuTouchKeyboard::Event::Type::Dismissed:
                Input::endTextInput();
                break;

            case UbuntuTouchKeyboard::Event::Type::Preedit:
                break;
            }
        }
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
