#include "raylib.h"
#include <audio.hpp>
#include <cmath>
#include <image.hpp>
#include <input.hpp>
#include <log.hpp>
#include <render.hpp>
#include <runtime.hpp>

// todo: actually finish this

bool Render::Init() {
    int windowWidth = 480;
    int windowHeight = 360;

    InitWindow(windowWidth, windowHeight, "Scratch Everywhere!");

}

void *Render::getRenderer() {
return static_cast<void *>(renderer);

void Render::deInit() {
    UnloadTexture(); //placeholder
    CloseWindow(); 
}

void *Render::getRenderer() {
    return nullptr;
}

void Render::setRenderTarget(void *renderTarget) {
}

void Render::clearRenderTarget() {
}

bool Render::createSpeechManager() {

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
    return true;
}
