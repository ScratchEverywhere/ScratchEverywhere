#include "window.hpp"
#if defined(_WIN32) || defined(_WIN64)
    #define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
    #define GLFW_EXPOSE_NATIVE_COCOA
#elif (defined(__linux__) && !defined(__ANDROID__)) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || (defined(__sun) && defined(__SVR4))
    #define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <libdlgmod/libdlgmod.h>
#include <algorithm>
#include <input.hpp>
#include <iostream>
#include <log.hpp>
#include <math.hpp>
#include <render.hpp>
#include <renderers/opengl/render.hpp>
#include <text.hpp>

static void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    globalWindow->resize(width, height);
    glViewport(0, 0, width, height);
}

bool WindowGLFW::init(int w, int h, const std::string &title) {
#if (defined(__linux__) && !defined(__ANDROID__)) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || (defined(__sun) && defined(__SVR4))
	glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif

    if (!glfwInit()) {
        Log::logError("Failed to initialize GLFW");
        return false;
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    window = glfwCreateWindow(w, h, title.c_str(), NULL, NULL);
    if (!window) {
        glfwTerminate();
        Log::logError("Failed to create GLFW window");
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glfwGetFramebufferSize(window, &width, &height);

#if defined(_WIN32) || defined(_WIN64)
	widget_set_owner(std::to_string((unsigned long long)(void *)glfwGetWin32Window(window)).c_str());
#elif defined(__APPLE__)
	widget_set_owner(std::to_string((unsigned long long)(void *)glfwGetCocoaWindow(window)).c_str());
#elif (defined(__linux__) && !defined(__ANDROID__)) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || (defined(__sun) && defined(__SVR4))
	widget_set_owner(std::to_string((unsigned long long)glfwGetX11Window(window)).c_str());
#endif

    return true;
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
