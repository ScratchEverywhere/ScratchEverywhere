#include "window.hpp"
#include "../render.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <input.hpp>
#include <iostream>
#include <math.hpp>
#include <render.hpp>
#include <text.hpp>

static void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    globalWindow->resize(width, height);
    glViewport(0, 0, width, height);
    Render::setRenderScale();
}

bool GlfwWindow::init(int w, int h, const std::string &title) {
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

    return true;
}

void GlfwWindow::cleanup() {
    glfwDestroyWindow(window);
    glfwTerminate();
}

bool GlfwWindow::shouldClose() {
    return glfwWindowShouldClose(window);
}

void GlfwWindow::pollEvents() {
    glfwPollEvents();
}

void GlfwWindow::swapBuffers() {
    glfwSwapBuffers(window);
}

void GlfwWindow::resize(int width, int height) {
    this->width = width;
    this->height = height;
}

int GlfwWindow::getWidth() const {
    return width;
}

int GlfwWindow::getHeight() const {
    return height;
}

void *GlfwWindow::getHandle() {
    return window;
}
