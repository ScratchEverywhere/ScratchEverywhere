#include "renderer_gl2d.hpp"
#include "speech_manager_gl2d.hpp"

#include <audio.hpp>
#include <fat.h>
#include <filesystem.h>
#include <gl2d.h>
#include <image_gl2d.hpp>
#include <input.hpp>
#include <nds.h>
#include <nds/arm9/dldi.h>
#include <unordered_map>
#include <window.hpp>
#include <windowing/nds/window_nds.hpp>

#define SCREEN_WIDTH 256
#define BOTTOM_SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192

#define SCREEN_HALF_WIDTH 132.5
#define SCREEN_HALF_HEIGHT 96

Renderer_GL2D::Renderer_GL2D() {
    window = createWindow(256, 192, "Scratch Everywhere!", WindowingBackends::NDS);
    if (window->error.has_value()) {
        error = "Failed to init window: " + window->error.value();
        delete window;
        window = nullptr;
        return;
    }
}

Renderer_GL2D::~Renderer_GL2D() {
    if (speechManager) {
        delete speechManager;
        speechManager = nullptr;
    }
    TextObject::cleanupText();

    if (window) {
        window->cleanup();
        delete window;
        window = nullptr;
    }
    SoundPlayer::deinit();
}

void Renderer_GL2D::setRenderTarget(void *renderTarget) {
}

void Renderer_GL2D::clearRenderTarget() {
}

bool Renderer_GL2D::createSpeechManager() {
    if (speechManager == nullptr) speechManager = new SpeechManagerGL2D();
    return speechManager != nullptr;
}

void Renderer_GL2D::destroySpeechManager() {
    delete speechManager;
    speechManager = nullptr;
}

SpeechManager *Renderer_GL2D::getSpeechManager() {
    return speechManager;
}

void Renderer_GL2D::beginFrame(int screen, int colorR, int colorG, int colorB) {
    if (hasFrameBegan) return;
    glBegin2D();
    int r5 = colorR >> 3;
    int g5 = colorG >> 3;
    int b5 = colorB >> 3;
    glClearColor(r5, g5, b5, 31);
    hasFrameBegan = true;
}

void Renderer_GL2D::endFrame(bool shouldFlush) {
    glEnd2D();
    glFlush(GL_TRANS_MANUALSORT);
    hasFrameBegan = false;
}

int Renderer_GL2D::getWidth() {
    return SCREEN_WIDTH;
}

int Renderer_GL2D::getHeight() {
    return SCREEN_HEIGHT;
}

float Renderer_GL2D::getPixelDensity() {
    return 1.0f;
}

bool Renderer_GL2D::initPen() {
    return false;
}

void Renderer_GL2D::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
}

void Renderer_GL2D::penDotAccurate(Sprite *sprite) {
}

void Renderer_GL2D::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
}

void Renderer_GL2D::penDotFast(Sprite *sprite) {
}

void Renderer_GL2D::penStamp(Sprite *sprite) {
}

void Renderer_GL2D::penClear() {
}

void Renderer_GL2D::renderSprites() {
    if (Render::renderMode == Render::BOTTOM_SCREEN_ONLY) lcdMainOnBottom();
    glBegin2D();
    glClearColor(31, 31, 31, 31);

    for (auto it = Scratch::sprites.rbegin(); it != Scratch::sprites.rend(); ++it) {
        Sprite *currentSprite = *it;
        if (!currentSprite->visible || currentSprite->ghostEffect > 70) continue;

        auto imgFind = Scratch::costumeImages.find(currentSprite->costumes[currentSprite->currentCostume].fullName);
        if (imgFind != Scratch::costumeImages.end()) {
            Image_GL2D *image = reinterpret_cast<Image_GL2D *>(imgFind->second.get());
            glBindTexture(GL_TEXTURE_2D, image->textureID);

            const bool isSVG = currentSprite->costumes[currentSprite->currentCostume].isSVG;
            Render::calculateRenderPosition(currentSprite, isSVG);
            if (!currentSprite->visible) continue;

            ImageRenderParams params;
            params.centered = true;
            params.x = currentSprite->renderInfo.renderX;
            params.y = currentSprite->renderInfo.renderY;
            params.rotation = currentSprite->renderInfo.renderRotation;
            params.scale = currentSprite->renderInfo.renderScaleY;
            params.flip = (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT && currentSprite->rotation < 0);

            image->render(params);
        }
    }

    if (speechManager) {
        speechManager->render();
    }

    if (Input::mousePointer.isMoving) {
        glBoxFilled((Input::mousePointer.x * Render::renderScale) + (SCREEN_HALF_WIDTH - 3), (-Input::mousePointer.y * Render::renderScale) + (SCREEN_HALF_HEIGHT - 3),
                    (Input::mousePointer.x * Render::renderScale) + (SCREEN_HALF_WIDTH + 3), (-Input::mousePointer.y * Render::renderScale) + (SCREEN_HALF_HEIGHT + 3),
                    RGB15(0, 0, 15));
        Input::mousePointer.x = std::clamp((float)Input::mousePointer.x, -Scratch::projectWidth * 0.5f, Scratch::projectWidth * 0.5f);
        Input::mousePointer.y = std::clamp((float)Input::mousePointer.y, -Scratch::projectHeight * 0.5f, Scratch::projectHeight * 0.5f);
    }

    Render::renderMonitors();
    glEnd2D();
    glFlush(GL_TRANS_MANUALSORT);
}

void Renderer_GL2D::renderPenLayer() {
}

void Renderer_GL2D::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    glBoxFilled(x - w / 2, y - h / 2, x + w / 2, y + h / 2, Math::color(colorR, colorG, colorB, colorA));
}

void Renderer_GL2D::drawRoundedBox(int w, int h, int x, int y, int radius, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    drawBox(w, h, x, y, colorR, colorG, colorB, colorA);
}

bool Renderer_GL2D::appShouldRun() {
    return true;
}