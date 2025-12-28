#include "window.hpp"
#include <iostream>

bool Window3DS::init(int w, int h, const std::string &title) {
    gfxInitDefault();
    romfsInit();
    
    C3D_Init(0x100000);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    
    return true;
}

void Window3DS::cleanup() {
    C2D_Fini();
    C3D_Fini();
    romfsExit();
    gfxExit();
}

bool Window3DS::shouldClose() {
    return !aptMainLoop();
}

void Window3DS::pollEvents() {
    hidScanInput();
}

void Window3DS::swapBuffers() {
    // 3DS buffers are swapped by C3D_FrameEnd which is called in Render::renderSprites
}

void Window3DS::resize(int width, int height) {
    // 3DS screen size is fixed
}

int Window3DS::getWidth() const {
    return width;
}

int Window3DS::getHeight() const {
    return height;
}

void *Window3DS::getHandle() {
    return nullptr;
}
