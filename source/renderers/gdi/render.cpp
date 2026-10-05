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

static HDC penLayer;
static RGBQUAD *penLayerQuad;
static HBITMAP penLayerBitmap;

static HDC penLayerMask;
static RGBQUAD *penLayerMaskQuad;
static HBITMAP penLayerMaskBitmap;

static int penWidth = 480;
static int penHeight = 360;

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

    penLayer = CreateCompatibleDC(renderer);
    penLayerBitmap = Image_GDI::NewBitmap(renderer, windowWidth, windowHeight, &penLayerQuad);
    SelectObject(penLayer, penLayerBitmap);

    penLayerMask = CreateCompatibleDC(renderer);
    penLayerMaskBitmap = Image_GDI::NewBitmap(renderer, windowWidth, windowHeight, &penLayerMaskQuad);
    SelectObject(penLayerMask, penLayerMaskBitmap);

    Render::penClear();

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
        DeleteObject(penLayerMaskBitmap);
        DeleteDC(penLayerMask);

        DeleteObject(penLayerBitmap);
        DeleteDC(penLayer);

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
    return 480;
}

int Render::getHeight() {
    if (globalWindow) return globalWindow->getHeight();
    return 360;
}

float Render::getPixelDensity() {
    if (globalWindow) return globalWindow->getPixelDensity();
    return 1.0f;
}

bool Render::initPen() {
    return true;
}

void Render::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
    Render::penMoveAccurate(x1, y1, x2, y2, sprite);
}

void Render::penDotFast(Sprite *sprite) {
    Render::penDotAccurate(sprite);
}

/* this thing cannot draw transparent pens but oh well... */
void Render::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    COLORREF rgb = RGB((DWORD)rgbColor.r, (DWORD)rgbColor.g, (DWORD)rgbColor.b);
    HPEN pen;
    double scale = (float)penHeight / Render::getHeight();
    double r = sprite->penData.size / scale;
    int y, x;
    int rW = Render::getWidth();
    int rH = Render::getHeight();

    x1 += rW * scale / 2;
    y1 = -y1 + rH * scale / 2;
    x2 += rW * scale / 2;
    y2 = -y2 + rH * scale / 2;

    x1 /= scale;
    y1 /= scale;
    x2 /= scale;
    y2 /= scale;

    x1 = round(x1);
    y1 = round(y1);
    x2 = round(x2);
    y2 = round(y2);

    pen = CreatePen(PS_SOLID, r, rgb);
    SelectObject(penLayer, pen);
    MoveToEx(penLayer, x1, y1, NULL);
    LineTo(penLayer, x2, y2);
    DeleteObject(pen);

    pen = CreatePen(PS_SOLID, r, RGB(0xff, 0xff, 0xff));
    SelectObject(penLayerMask, pen);
    MoveToEx(penLayerMask, x1, y1, NULL);
    LineTo(penLayerMask, x2, y2);
    DeleteObject(pen);
}

void Render::penDotAccurate(Sprite *sprite) {
    Render::penMoveAccurate(sprite->xPosition, sprite->yPosition, sprite->xPosition, sprite->yPosition, sprite);
}

void Render::penStamp(Sprite *sprite) {
    const auto &imgFind = Scratch::costumeImages.find(sprite->costumes[sprite->currentCostume].fullName);
    int rW = Render::getWidth();
    int rH = Render::getHeight();
    Image_GDI *image;
    bool isSVG;
    double scale = (float)penHeight / Render::getHeight();
    ImageRenderParams params;
    if (imgFind == Scratch::costumeImages.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return;
    }

    image = reinterpret_cast<Image_GDI *>(imgFind->second.get());

    isSVG = sprite->costumes[sprite->currentCostume].isSVG;
    Render::calculateRenderPosition(sprite, isSVG);

    params.centered = true;
    params.x = sprite->renderInfo.renderX / scale;
    params.y = sprite->renderInfo.renderY / scale;
    params.scale = sprite->renderInfo.renderScaleY;
    params.rotation = sprite->renderInfo.renderRotation;
    params.flip = (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0);
    params.opacity = 1.0f - (std::clamp(sprite->ghostEffect, 0.0f, 100.0f) * 0.01f);
    params.brightness = sprite->brightnessEffect;

    image->render(params, penLayer);
}

void Render::penClear() {
    PatBlt(penLayer, 0, 0, Render::getWidth(), Render::getHeight(), BLACKNESS);
    PatBlt(penLayerMask, 0, 0, Render::getWidth(), Render::getHeight(), BLACKNESS);
}

void Render::beginFrame(int screen, int colorR, int colorG, int colorB) {
    if (!hasFrameBegan) {
        HBRUSH brush = CreateSolidBrush(RGB(colorR, colorG, colorB));
        RECT rc;

        rc.left = 0;
        rc.top = 0;
        rc.right = Render::getWidth();
        rc.bottom = Render::getHeight();

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

void Render::renderPenLayer() {
    POINT p[3];
    int i;

    p[0].x = 0;
    p[0].y = 0;
    p[1].x = Render::getWidth();
    p[1].y = 0;
    p[2].x = 0;
    p[2].y = Render::getHeight();

    for (i = 0; i < p[1].x * p[2].y; i++) {
        if (penLayerMaskQuad[i].rgbRed) {
            penLayerQuad[i].rgbReserved = 0xff;
        }
    }

    Image_GDI::PlgAlphaBlt(renderer, p, penLayer, 0, 0, p[1].x, p[2].y, 255);
}

void Render::renderSprites() {
    Render::beginFrame(0, 255, 255, 255);

    for (auto it = Scratch::sprites.rbegin(); it != Scratch::sprites.rend(); ++it) {
        Sprite *currentSprite = *it;

        auto imgFind = Scratch::costumeImages.find(currentSprite->costumes[currentSprite->currentCostume].fullName);
        if (imgFind != Scratch::costumeImages.end()) {
            Image_GDI *image = reinterpret_cast<Image_GDI *>(imgFind->second.get());

            const bool isSVG = currentSprite->costumes[currentSprite->currentCostume].isSVG;
            ImageRenderParams params;
            Render::calculateRenderPosition(currentSprite, isSVG);
            if (!currentSprite->visible) continue;

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

    Render::endFrame(true);
}

bool Render::appShouldRun() {
    if (OS::toExit) return false;
    if (globalWindow) {
        globalWindow->pollEvents();
        return !globalWindow->shouldClose();
    }
    return false;
}
