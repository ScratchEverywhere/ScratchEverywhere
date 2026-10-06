#include "raylib.h"

#include <audio.hpp>
#include <cmath>
#include <image.hpp>
#include <input.hpp>
#include <log.hpp>
#include <render.hpp>
#include <runtime.hpp>
#include <windowing/glfw/window_glfw.hpp>

// todo: actually finish this

bool Render::Init() {
    int windowWidth = 480;
    int windowHeight = 360;

    globalWindow = new WindowGLFW();
    if (!globalWindow->init(windowWidth, windowHeight, "Scratch Everywhere!")) {
        delete globalWindow;
        globalWindow = nullptr;
        return false;
    }
    
    return true;
}

}

void *Render::getRenderer() {
return static_cast<void *>(renderer);
}

void Render::deInit() {
    if (speechManager) {
        delete speechManager;
        speechManager = nullptr;
    }
    TextObject::cleanupText();

    if (globalWindow) {
        globalWindow->cleanup();
        delete globalWindow;
        globalWindow = nullptr;
    }
    SoundPlayer::deinit();
    CloseWindow(); 
}

void *Render::getRenderer() {
    return nullptr;
}

void Render::setRenderTarget(void *renderTarget) {
    // umm idk what is this for?
}

void Render::clearRenderTarget() {
}

bool Render::createSpeechManager() {
    if (speechManager == nullptr) speechManager = new SpeechManagerRaylib();
    return speechManager != nullptr;
}

void Render::destroySpeechManager() {
    delete speechManager;
    speechManager = nullptr;
}

SpeechManager *Render::getSpeechManager() {
    return speechManager;
}

void Render::beginFrame(int screen, int colorR, int colorG, int colorB) {

}

void Render::endFrame(bool shouldFlush) {

}

int Render::getWidth() {
    return windowWidth;
}

int Render::getHeight() {
    return windowHeight;
}

float Render::getPixelDensity() {
    return 1.0f;
}

bool Render::initPen() {
    return false;
}

void Render::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
}

void Render::penDotAccurate(Sprite *sprite) {
}

void Render::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
}

void Render::penDotFast(Sprite *sprite) {
}

void Render::penStamp(Sprite *sprite) {
}

void Render::penClear() {
}

void Render::renderSprites() {
        }
    }

    if (speechManager) {
    }

    if (Input::mousePointer.isMoving) {

    }

}

void Render::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {

}

bool Render::appShouldRun() {
    if (OS::toExit) return false;
    if (globalWindow) {
        globalWindow->pollEvents();
        return !globalWindow->shouldClose();
    }
    return false;
}