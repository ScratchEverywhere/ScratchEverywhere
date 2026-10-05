#include "image_gdi.hpp"
#include "render_gdi.hpp"
#include "speech_manager.hpp"
#include "speech_manager_gdi.hpp"
#include "types.hpp"
#define WIN32_LEAN_AND_MEAN
#include <algorithm>
#include <audio.hpp>
#include <cmath>
#include <image.hpp>
#include <input.hpp>
#include <log.hpp>
#include <render.hpp>
#include <runtime.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <windowing/win32/window_win32.hpp>
#include <windows.h>

WindowSE *globalWindow = nullptr;

SpeechManagerGDI *speechManager = nullptr;

HDC renderer = nullptr;

bool Render::Init() {
    int windowWidth = 480;
    int windowHeight = 360;

    globalWindow = new WindowWin32();
    if (!globalWindow->init(windowWidth, windowHeight, "Scratch Everywhere!")) {
        delete globalWindow;
        globalWindow = nullptr;
        return false;
    }

    renderer = ((WindowWin32 *)globalWindow)->hDC;

    debugMode = true;

    return true;
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
    ExitProcess(0);
}

void *Render::getRenderer() {
    return nullptr;
}

void Render::setRenderTarget(void *renderTarget) {
}

void Render::clearRenderTarget() {
}

bool Render::createSpeechManager() {
    if (speechManager == nullptr) speechManager = new SpeechManagerGDI();
    return speechManager != nullptr;
}

void Render::destroySpeechManager() {
    delete speechManager;
    speechManager = nullptr;
}

SpeechManager *Render::getSpeechManager() {
    return speechManager;
}

int Render::getWidth() {
    if (globalWindow) return globalWindow->getWidth();
    return 540;
}

int Render::getHeight() {
    if (globalWindow) return globalWindow->getHeight();
    return 405;
}

float Render::getPixelDensity() {
    if (globalWindow) return globalWindow->getPixelDensity();
    return 1.0f;
}

bool Render::initPen() {
    return true;
}

void Render::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
}

void Render::penDotFast(Sprite *sprite) {
}

void Render::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
}

void Render::penDotAccurate(Sprite *sprite) {
}

void Render::penStamp(Sprite *sprite) {
}

void Render::penClear() {
}

void Render::beginFrame(int screen, int colorR, int colorG, int colorB) {
    if (!hasFrameBegan) {
        HBRUSH brush = CreateSolidBrush(RGB(colorR, colorG, colorB));
        RECT rc;

        rc.left = 0;
        rc.top = 0;
        rc.right = 480;
        rc.bottom = 360;

        FillRect(renderer, &rc, brush);
        DeleteObject(brush);
        hasFrameBegan = true;
    }
}

void Render::endFrame(bool shouldFlush) {
    Sleep(16);
    if (globalWindow) globalWindow->swapBuffers();
    hasFrameBegan = false;
}

void Render::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    HBRUSH brush = CreateSolidBrush(RGB(colorR, colorG, colorB));
    RECT rc;

    rc.left = x - w / 2;
    rc.top = y - h / 2;
    rc.right = x + w / 2;
    rc.bottom = y + h / 2;

    FillRect(renderer, &rc, brush);
    DeleteObject(brush);
}

void Render::renderSprites() {
    for (auto it = Scratch::sprites.rbegin(); it != Scratch::sprites.rend(); ++it) {
        Sprite *currentSprite = *it;

        auto imgFind = Scratch::costumeImages.find(currentSprite->costumes[currentSprite->currentCostume].fullName);
        if (imgFind != Scratch::costumeImages.end()) {
            Image_GDI *image = reinterpret_cast<Image_GDI *>(imgFind->second.get());

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
            params.opacity = 1.0f - (std::clamp(currentSprite->ghostEffect, 0.0f, 100.0f) * 0.01f);
            params.brightness = currentSprite->brightnessEffect;

            image->render(params);
        }

        if (currentSprite->isStage) Render::renderPenLayer();
    }

    if (speechManager) {
        speechManager->render();
    }

    Render::renderMonitors();
}

void Render::renderPenLayer() {
}

bool Render::appShouldRun() {
    if (OS::toExit) return false;
    if (globalWindow) {
        globalWindow->pollEvents();
        return !globalWindow->shouldClose();
    }
    return false;
}
