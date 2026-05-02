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

bool WindowRetroarch::init(int w, int h, const std::string &title) {
    resize(w, h);

    return true;
}

void WindowRetroarch::cleanup() {
}

bool WindowRetroarch::shouldClose() {
    return false;
}

void WindowRetroarch::pollEvents() {
    input_poll_cb();
}

void WindowRetroarch::swapBuffers() {
    video_refresh_cb(RETRO_HW_FRAME_BUFFER_VALID, width, height, 0);
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
