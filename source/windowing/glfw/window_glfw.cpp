#if (defined(_WIN32) || defined(_WIN64)) && !defined(GLFW_EXPOSE_NATIVE_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__) && !defined(GLFW_EXPOSE_NATIVE_COCOA)
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#include "window_glfw.hpp"
#if defined(_WIN32) || defined(_WIN64) || defined(__APPLE__)
#include <GLFW/glfw3native.h>
#include <libdlgmod/libdlgmod.h>
#endif
#include "input_glfw.hpp"
#include <algorithm>
#include <input.hpp>
#include <iostream>
#include <log.hpp>
#include <math.hpp>
#include <renderer.hpp>
#include <renderers/opengl/renderer_gl.hpp>
#include <text.hpp>
#include <utf8.hpp>

static void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    Render::getGlobalRenderer()->window->resize(width, height);
    glViewport(0, 0, width, height);
}

static void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    Input::mouseScrollDelta = 0;
    static double accumulated = 0.0;
    accumulated += yoffset;
    const int ticks = static_cast<int>(accumulated);
    if (ticks != 0) {
        Input::mouseScrollDelta += ticks;
        accumulated -= ticks;
    }
}

static void appendUtf8(std::string &out, unsigned int cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

static void char_callback(GLFWwindow *, unsigned int codepoint) {
    if (!Input::isTextInputActive() || codepoint < 32 || codepoint == 127) return;
    appendUtf8(Input::inputText, codepoint);
}

static void key_callback(GLFWwindow *window, int key, int, int action, int mods) {
    if (action == GLFW_RELEASE || !Input::isTextInputActive()) return;
    switch (key) {
    case GLFW_KEY_ENTER:
    case GLFW_KEY_KP_ENTER:
    case GLFW_KEY_ESCAPE:
        if (action == GLFW_PRESS) Input::endTextInput();
        break;
    case GLFW_KEY_BACKSPACE:
        utf8::eraseLast(Input::inputText, 1);
        break;
    case GLFW_KEY_V:
#ifdef __APPLE__
        if (mods & GLFW_MOD_SUPER) {
#else
        if (mods & GLFW_MOD_CONTROL) {
#endif
            if (const char *clip = glfwGetClipboardString(window)) {
                std::string pasted(clip);
                pasted.erase(std::remove(pasted.begin(), pasted.end(), '\r'), pasted.end());
                pasted.erase(std::remove(pasted.begin(), pasted.end(), '\n'), pasted.end());
                Input::inputText += pasted;
            }
        }
        break;
    }
}

WindowGLFW::WindowGLFW(int w, int h, const std::string &title) {
    Input::setGlobalInputDevice(new InputDevice_GLFW);
    if (!glfwInit()) {
        error = "Failed to initialize GLFW";
        return;
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);
    glfwWindowHint(GLFW_SAMPLES, WindowSE::msaaSamples);

    if (Render::currentRenderer == Renderers::OpenGL) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    } else if (Render::currentRenderer == Renderers::OpenGLCore) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    }

    window = glfwCreateWindow(w, h, title.c_str(), NULL, NULL);
    if (!window) {
        glfwTerminate();
        error = "Failed to create GLFW window";
        return;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetCharCallback(window, char_callback);
    glfwSetKeyCallback(window, key_callback);

    glfwGetFramebufferSize(window, &width, &height);

#if defined(_WIN32) || defined(_WIN64)
    widget_set_owner(std::to_string((unsigned long long)(void *)glfwGetWin32Window(window)).c_str());
#elif defined(__APPLE__)
    widget_set_owner(std::to_string((unsigned long long)(void *)glfwGetCocoaWindow(window)).c_str());
#endif
}

void WindowGLFW::cleanup() {
    glfwDestroyWindow(window);
    glfwTerminate();
}

bool WindowGLFW::shouldClose() {
    return glfwWindowShouldClose(window);
}

void WindowGLFW::pollEvents() {
    glfwPollEvents();
}

void WindowGLFW::swapBuffers() {
    glfwSwapBuffers(window);
}

void WindowGLFW::resize(int width, int height) {
    this->width = width;
    this->height = height;

    Render::setRenderScale();
    Render::resizeSVGs();
}

int WindowGLFW::getWidth() const {
    return width;
}

int WindowGLFW::getHeight() const {
    return height;
}

float WindowGLFW::getPixelDensity() const {
    return 1.0f;
}

void *WindowGLFW::getHandle() {
    return window;
}
