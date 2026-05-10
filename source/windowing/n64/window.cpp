#include "window.hpp"
#include <libdragon.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/gl_integration.h>
#include <render.hpp>

bool WindowN64::init(int w, int h, const std::string &title) {
    width = 320;
    height = 240;

    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    

    return true;
}

void WindowN64::cleanup() {
    gl_close();
    rdpq_close();
    display_close();
}

bool WindowN64::shouldClose() {
    return false; 
}

void WindowN64::pollEvents() {
    joypad_poll();
}

void WindowN64::swapBuffers() {
    gl_context_end();
    rdpq_detach_show();
}

void WindowN64::resize(int width, int height) {
    this->width = 320;
    this->height = 240;

    Render::setRenderScale();
    Render::resizeSVGs();
    
    glViewport(0, 0, 320, 240);
}

int WindowN64::getWidth() const {
    return width;
}

int WindowN64::getHeight() const {
    return height;
}

void *WindowN64::getHandle() {
    // On N64, the current display surface acts as the "window handle"
    return (void*)display_get();
}
