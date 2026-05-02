#include "window.hpp"
#include <algorithm>
#include <input.hpp>
#include <iostream>
#include <math.hpp>
#include <render.hpp>
#include <renderers/opengl/render.hpp>
#include <text.hpp>

bool WindowRetroarch::init(int w, int h, const std::string &title) {
    return true;
}

void WindowRetroarch::cleanup() {
}

bool WindowRetroarch::shouldClose() {
    return false;
}

void WindowRetroarch::pollEvents() {
}

void WindowRetroarch::swapBuffers() {
}

void WindowRetroarch::resize(int width, int height) {
    this->width = width;
    this->height = height;

    Render::setRenderScale();
    Render::resizeSVGs();
}

int WindowRetroarch::getWidth() const {
    return width;
}

int WindowRetroarch::getHeight() const {
    return height;
}

void *WindowRetroarch::getHandle() {
    return nullptr;
}
