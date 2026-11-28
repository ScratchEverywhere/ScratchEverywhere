#include "SDL_gamecontroller.h"
#if defined(RENDERER_OPENGL_CORE)
#include <renderers/opengl_core/renderer_gl_core.hpp>
#endif
#ifdef RENDERER_OPENGL
#include <renderers/opengl/renderer_gl.hpp>
#endif
#include "input_sdl2.hpp"
#include "window_sdl2.hpp"
#if defined(_WIN32) || defined(_WIN64) || defined(__APPLE__)
#include <SDL_syswm.h>
#include <libdlgmod/libdlgmod.h>
#endif
#include <input.hpp>
#include <log.hpp>
#include <math.hpp>
#include <os.hpp>
#include <renderer.hpp>
#ifdef RENDERER_SDL2
#include <renderers/sdl2/renderer_sdl2.hpp>
#endif
#include <algorithm>
#include <utf8.hpp>
#ifdef __UBUNTUTOUCH__
#include <platforms/ut/swkbd_ut.hpp>
extern bool utKeyboardShown;
UbuntuTouchKeyboard &getUbuntuTouchKeyboard();
#endif

#ifdef PLATFORM_HAS_CONTROLLER
SDL_GameController *controller = nullptr;
#endif

#ifdef PLATFORM_HAS_TOUCH
bool touchActive = false;
SDL_Point touchPosition;
#endif

WindowSDL2::WindowSDL2(int width, int height, const std::string &title) {
    Input::setGlobalInputDevice(new InputDevice_SDL2);
#if defined(VITA)
    SDL_setenv("VITA_DISABLE_TOUCH_BACK", "1", 1);
#elif defined(__UT1604__)
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "mir");
#endif

    uint32_t sdlFlags = 0;
// SDL has to be initialized before window creation on webOS
#ifndef WEBOS
    sdlFlags = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
#ifdef PLATFORM_HAS_CONTROLLER
    sdlFlags |= SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER;
#endif
    if (SDL_Init(sdlFlags) < 0) {
        error = "Failed to initialize SDL2: " + std::string(SDL_GetError());
        return;
    }
#endif

    if (Render::currentRenderer == Renderers::OpenGL) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    } else if (Render::currentRenderer == Renderers::OpenGLCore) {
#if defined(__EMSCRIPTEN__) || defined(__PS4__)
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#ifndef __SWITCH__
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    }

#ifdef WEBOS
    Uint32 flags = SDL_WINDOW_FULLSCREEN | SDL_WINDOW_ALLOW_HIGHDPI;
#else
    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#endif

    if (Render::currentRenderer == Renderers::OpenGL || Render::currentRenderer == Renderers::OpenGLCore)
        flags |= SDL_WINDOW_OPENGL;

    window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);
    if (!window) {
        error = "Failed to create SDL2 window: " + std::string(SDL_GetError());
        return;
    }

    if (Render::currentRenderer == Renderers::OpenGL || Render::currentRenderer == Renderers::OpenGLCore) {
        context = SDL_GL_CreateContext(window);
        if (!context) {
            error = "Failed to create OpenGL context: " + std::string(SDL_GetError());
            return;
        }

        SDL_GL_SetSwapInterval(1); // VSync

#if defined(RENDERER_OPENGL_CORE) && !defined(__EMSCRIPTEN__) && !defined(__PS4__)
#ifdef __SWITCH__
        if (!gladLoadGL()) {
#else
        if (!gladLoaderLoadGL()) {
#endif
            error = "Failed to initialize GLAD";
            return;
        }
#endif
    }

#ifdef PLATFORM_HAS_CONTROLLER
    if (SDL_NumJoysticks() > 0) controller = SDL_GameControllerOpen(0);
#endif

    this->width = width;
    this->height = height;
    calculatePixelDensity();

    if (Render::currentRenderer == Renderers::OpenGL || Render::currentRenderer == Renderers::OpenGLCore) {
        int dw, dh;
        SDL_GL_GetDrawableSize(window, &dw, &dh);
        resize(dw, dh);
    } else {
        int dw, dh;
#if defined(WEBOS) || defined(__UBUNTUTOUCH__)
        SDL_GetWindowSize(window, &dw, &dh);
#else
        SDL_GetWindowSizeInPixels(window, &dw, &dh);
#endif
        resize(dw, dh);
    }

#if defined(_WIN32) || defined(_WIN64) || defined(__APPLE__)
    SDL_SysWMinfo system_info;
    SDL_VERSION(&system_info.version);
    SDL_GetWindowWMInfo(window, &system_info);
#if defined(_WIN32) || defined(_WIN64)
    widget_set_owner(std::to_string((unsigned long long)(void *)system_info.info.win.window).c_str());
#elif defined(__APPLE__)
    widget_set_owner(std::to_string((unsigned long long)(void *)system_info.info.cocoa.window).c_str());
#endif
#endif

    // Print SDL version number. could be useful for debugging
    SDL_version ver;
    SDL_VERSION(&ver);
    Log::log("SDL v" + std::to_string(ver.major) + "." + std::to_string(ver.minor) + "." + std::to_string(ver.patch));
}

void WindowSDL2::cleanup() {
#ifdef PLATFORM_HAS_CONTROLLER
    if (controller) SDL_GameControllerClose(controller);
#endif

    if (Render::currentRenderer == Renderers::OpenGL || Render::currentRenderer == Renderers::OpenGLCore)
        SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
}

bool WindowSDL2::shouldClose() {
    return shouldCloseFlag;
}

void WindowSDL2::pollEvents() {
    Input::mouseScrollDelta = 0;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            OS::toExit = true;
            shouldCloseFlag = true;
            break;
#ifdef SE_FILE_DROP
        case SDL_DROPFILE:
            if (event.drop.file) {
                if (OS::pendingOpenFile.empty()) OS::pendingOpenFile = event.drop.file;
                SDL_free(event.drop.file);
            }
            break;
#endif
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                int w, h;
                if (Render::currentRenderer == Renderers::OpenGL || Render::currentRenderer == Renderers::OpenGLCore)
                    SDL_GL_GetDrawableSize(window, &w, &h);
                else {
#if defined(WEBOS) || defined(__UBUNTUTOUCH__)
                    SDL_GetWindowSize(window, &w, &h);
#else
                    SDL_GetWindowSizeInPixels(window, &w, &h);
#endif
                }
                resize(w, h);
            }
            break;
#ifdef PLATFORM_HAS_CONTROLLER
        case SDL_CONTROLLERDEVICEADDED: {
            if (!controller) controller = SDL_GameControllerOpen(event.cdevice.which);

            if (!controller) {
                Log::logError("Failed to connect controller: " + std::string(SDL_GetError()));
                break;
            }

            const char *name = SDL_GameControllerName(controller);
            if (!name) break;
            Log::log("[SDL2] Controller connected: " + std::string(name));
            if (SDL_strstr(name, "Wii")) {
                Input::setControllerType(ControllerType::WiiRemote);
            } else if (SDL_strstr(name, "Nintendo")) {
                Input::setControllerType(ControllerType::Nintendo);
            } else if (SDL_strstr(name, "Xbox")) {
                Input::setControllerType(ControllerType::Xbox);
            } else if (SDL_strstr(name, "Playstation") || SDL_strstr(name, "PS")) {
                Input::setControllerType(ControllerType::Playstation);
            }
            break;
        }
        case SDL_CONTROLLERDEVICEREMOVED:
            if (controller && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))) {
                SDL_GameControllerClose(controller);
                controller = nullptr;
            }
            Input::setControllerType(ControllerType::Unknown);
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
#if defined(__PC__)
        case SDL_MOUSEWHEEL:
            Input::mouseScrollDelta += event.wheel.y;
            break;
#endif
#ifndef SDL2_NO_TEXT_INPUT
        case SDL_TEXTINPUT:
            Input::inputText += event.text.text;
            break;
        case SDL_KEYDOWN:
            if (!Input::isTextInputActive()) break;
            switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_RETURN:
            case SDL_SCANCODE_KP_ENTER:
            case SDL_SCANCODE_ESCAPE:
                Input::endTextInput();
                break;
            case SDL_SCANCODE_BACKSPACE:
                utf8::eraseLast(Input::inputText, 1);
                break;
            case SDL_SCANCODE_V: {
#ifdef __APPLE__
                const bool pasteMod = (event.key.keysym.mod & KMOD_GUI) != 0;
#else
                const bool pasteMod = (event.key.keysym.mod & KMOD_CTRL) != 0;
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
            } break;
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

void WindowSDL2::calculatePixelDensity() {
#if !defined(WEBOS) && !defined(__UBUNTUTOUCH__)
    int logicalW, logicalH, pixelW, pixelH;

    SDL_GetWindowSize(window, &logicalW, &logicalH);
    SDL_GetWindowSizeInPixels(window, &pixelW, &pixelH);

    this->pixelDensity = static_cast<float>(pixelW) / logicalW;
#endif
}

void WindowSDL2::swapBuffers() {
    if (Render::currentRenderer == Renderers::OpenGL || Render::currentRenderer == Renderers::OpenGLCore)
        SDL_GL_SwapWindow(window);
}

void WindowSDL2::resize(int width, int height) {
    this->width = width;
    this->height = height;
#if defined(RENDERER_OPENGL) || defined(RENDERER_OPENGL_CORE)
    if (Render::currentRenderer == Renderers::OpenGL || Render::currentRenderer == Renderers::OpenGLCore)
        glViewport(0, 0, width, height);
#endif

    calculatePixelDensity();

    Render::setRenderScale();
    Render::resizeSVGs();
}

int WindowSDL2::getWidth() const {
    return width;
}

int WindowSDL2::getHeight() const {
    return height;
}

float WindowSDL2::getPixelDensity() const {
    return pixelDensity;
}

void *WindowSDL2::getHandle() {
    return window;
}
