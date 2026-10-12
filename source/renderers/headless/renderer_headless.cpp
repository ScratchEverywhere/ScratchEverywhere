#include "renderer_headless.hpp"

Renderer_Headless::Renderer_Headless() {
}

Renderer_Headless::~Renderer_Headless() = default;

bool Renderer_Headless::initPen() {
    return true;
}

void Renderer_Headless::setRenderTarget(void *renderTarget) {
}

void Renderer_Headless::clearRenderTarget() {
}

bool Renderer_Headless::createSpeechManager() {
    return true;
}

SpeechManager *Renderer_Headless::getSpeechManager() {
    return nullptr;
}

void Renderer_Headless::destroySpeechManager() {
}

void Renderer_Headless::beginFrame(int screen, int colorR, int colorG, int colorB) {
    hasFrameBegan = true;
}

void Renderer_Headless::endFrame(bool shouldFlush) {
    hasFrameBegan = false;
}

int Renderer_Headless::getWidth() {
    return 0;
}

int Renderer_Headless::getHeight() {
    return 0;
}

float Renderer_Headless::getPixelDensity() {
    return 1.0f;
}

void Renderer_Headless::renderSprites() {
}

void Renderer_Headless::renderPenLayer() {
}

void Renderer_Headless::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
}

void Renderer_Headless::drawRoundedBox(int w, int h, int x, int y, int radius, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
}

bool Renderer_Headless::appShouldRun() {
    return true;
}

void Renderer_Headless::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
}

void Renderer_Headless::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
}

void Renderer_Headless::penDotFast(Sprite *sprite) {
}

void Renderer_Headless::penDotAccurate(Sprite *sprite) {
}

void Renderer_Headless::penStamp(Sprite *sprite) {
}

void Renderer_Headless::penClear() {
}