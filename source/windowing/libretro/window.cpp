#include "window.hpp"
#include <libretro.h>

#include <algorithm>
#include <input.hpp>
#include <iostream>
#include <math.hpp>
#include <render.hpp>
#include <renderers/opengl/render.hpp>
#include <text.hpp>

static retro_video_refresh_t video_refresh_cb;
static retro_input_poll_t input_poll_cb;

extern "C" void retro_set_input_poll(retro_input_poll_t cb) {
    input_poll_cb = cb;
}

extern "C" void retro_set_video_refresh(retro_video_refresh_t cb) {
    video_refresh_cb = cb;
}

bool WindowLibretro::init(int w, int h, const std::string &title) {
    resize(w, h);

    return true;
}

void WindowLibretro::cleanup() {
}

bool WindowLibretro::shouldClose() {
    return false;
}

void WindowLibretro::pollEvents() {
    input_poll_cb();
}

void WindowLibretro::swapBuffers() {
    video_refresh_cb(RETRO_HW_FRAME_BUFFER_VALID, width, height, 0);
}

void WindowLibretro::resize(int width, int height) {
    this->width = width;
    this->height = height;

    Render::setRenderScale();
    Render::resizeSVGs();
}

int WindowLibretro::getWidth() const {
    return width;
}

int WindowLibretro::getHeight() const {
    return height;
}

void *WindowLibretro::getHandle() {
    return nullptr;
}
