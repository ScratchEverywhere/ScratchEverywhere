#include "renderer_gdi.hpp"
#include "image_gdi.hpp"
#include "speech_manager_gdi.hpp"

#define WIN32_LEAN_AND_MEAN
#include <algorithm>
#include <audio.hpp>
#include <cmath>
#include <image.hpp>
#include <input.hpp>
#include <log.hpp>
#include <runtime.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <window.hpp>
#include <windowing/win32/window_win32.hpp>
#include <windows.h>

HDC renderer = nullptr;
HDC rendererWin = nullptr;

namespace {
HDC penLayer;
RGBQUAD *penLayerQuad;
HBITMAP penLayerBitmap;

HDC penLayerMask;
RGBQUAD *penLayerMaskQuad;
HBITMAP penLayerMaskBitmap;

int penWidth = 480;
int penHeight = 360;

void resized(int width, int height) {
    if (Scratch::hqpen) {
        if (Scratch::projectWidth / static_cast<double>(width) <
            Scratch::projectHeight / static_cast<double>(height)) {
            penWidth = Scratch::projectWidth * (height / static_cast<double>(Scratch::projectHeight));
            penHeight = height;
        } else {
            penWidth = width;
            penHeight = Scratch::projectHeight * (width / static_cast<double>(Scratch::projectWidth));
        }
    } else {
        penWidth = Scratch::projectWidth;
        penHeight = Scratch::projectHeight;
    }

    DeleteObject(penLayerBitmap);
    penLayerBitmap = Image_GDI::NewBitmap(rendererWin, width, height, &penLayerQuad);
    SelectObject(penLayer, penLayerBitmap);

    DeleteObject(penLayerMaskBitmap);
    penLayerMaskBitmap = Image_GDI::NewBitmap(rendererWin, width, height, &penLayerMaskQuad);
    SelectObject(penLayerMask, penLayerMaskBitmap);
}

void drawBlackBars(int screenWidth, int screenHeight) {
    float screenAspect = static_cast<float>(screenWidth) / screenHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;
    HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));

    if (screenAspect > projectAspect) {
        /* vertical */
        RECT rc;
        float sw = (float)screenHeight / Scratch::projectHeight * Scratch::projectWidth;
        float w = screenWidth - sw;

        rc.left = 0;
        rc.top = 0;
        rc.right = w / 2;
        rc.bottom = screenHeight;
        FillRect(renderer, &rc, brush);

        rc.left += w / 2 + sw;
        rc.right = screenWidth;
        FillRect(renderer, &rc, brush);
    } else {
        /* horizontal */
        RECT rc;
        float sh = (float)screenWidth / Scratch::projectWidth * Scratch::projectHeight;
        float h = screenHeight - sh;

        rc.left = 0;
        rc.top = 0;
        rc.right = screenWidth;
        rc.bottom = h / 2;

        FillRect(renderer, &rc, brush);

        rc.top += h / 2 + sh;
        rc.bottom = screenHeight;
        FillRect(renderer, &rc, brush);
    }
    DeleteObject(brush);
}
} // namespace

Renderer_GDI::Renderer_GDI() {
    int windowWidth = 480;
    int windowHeight = 360;

    this->window = createWindow(windowWidth, windowHeight, "Scratch Everywhere!", WindowingBackends::Win32);
    if (this->window->error.has_value()) {
        error = "Failed to init window: " + window->error.value();
        delete window;
        window = nullptr;
        return;
    }

    renderer = ((WindowWin32 *)this->window)->hDC;
    rendererWin = ((WindowWin32 *)this->window)->winDC;

    penLayer = CreateCompatibleDC(rendererWin);
    penLayerBitmap = Image_GDI::NewBitmap(rendererWin, windowWidth, windowHeight, &penLayerQuad);
    SelectObject(penLayer, penLayerBitmap);

    penLayerMask = CreateCompatibleDC(rendererWin);
    penLayerMaskBitmap = Image_GDI::NewBitmap(rendererWin, windowWidth, windowHeight, &penLayerMaskQuad);
    SelectObject(penLayerMask, penLayerMaskBitmap);

    penClear();
}

Renderer_GDI::~Renderer_GDI() {
    if (this->speechManager) {
        delete this->speechManager;
        this->speechManager = nullptr;
    }

    TextObject::cleanupText();

    if (this->window) {
        DeleteObject(penLayerMaskBitmap);
        DeleteDC(penLayerMask);

        DeleteObject(penLayerBitmap);
        DeleteDC(penLayer);

        this->window->cleanup();
        delete this->window;
        this->window = nullptr;
    }

    SoundPlayer::deinit();
    ExitProcess(0);
}

void Renderer_GDI::setRenderTarget(void *renderTarget, int width, int height) {
}

void Renderer_GDI::clearRenderTarget() {
}

bool Renderer_GDI::createSpeechManager() {
    if (this->speechManager == nullptr) this->speechManager = new SpeechManagerGDI();
    return this->speechManager != nullptr;
}

void Renderer_GDI::destroySpeechManager() {
    delete this->speechManager;
    this->speechManager = nullptr;
}

SpeechManager *Renderer_GDI::getSpeechManager() {
    return this->speechManager;
}

int Renderer_GDI::getWidth() {
    if (this->window) return this->window->getWidth();
    return 480;
}

int Renderer_GDI::getHeight() {
    if (this->window) return this->window->getHeight();
    return 360;
}

float Renderer_GDI::getPixelDensity() {
    if (this->window) return this->window->getPixelDensity();
    return 1.0f;
}

bool Renderer_GDI::initPen() {
    return true;
}

void Renderer_GDI::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
    penMoveAccurate(x1, y1, x2, y2, sprite);
}

void Renderer_GDI::penDotFast(Sprite *sprite) {
    penDotAccurate(sprite);
}

/* this thing cannot draw transparent pens but oh well... */
void Renderer_GDI::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    COLORREF rgb = RGB((DWORD)rgbColor.r, (DWORD)rgbColor.g, (DWORD)rgbColor.b);
    HPEN pen;
    double scale = (this->getHeight() < this->getWidth()) ? ((float)Scratch::projectHeight / this->getHeight()) : ((float)Scratch::projectWidth / this->getWidth());
    double r = sprite->penData.size / scale;
    int y, x;
    int rW = this->getWidth();
    int rH = this->getHeight();

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

void Renderer_GDI::penDotAccurate(Sprite *sprite) {
    penMoveAccurate(sprite->xPosition, sprite->yPosition, sprite->xPosition, sprite->yPosition, sprite);
}

void Renderer_GDI::penStamp(Sprite *sprite) {
    const auto &imgFind = Scratch::costumeImages.find(sprite->costumes[sprite->currentCostume].fullName);
    int rW = this->getWidth();
    int rH = this->getHeight();
    Image_GDI *image;
    bool isSVG;
    ImageRenderParams params;
    if (imgFind == Scratch::costumeImages.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return;
    }

    image = reinterpret_cast<Image_GDI *>(imgFind->second.get());

    isSVG = sprite->costumes[sprite->currentCostume].isSVG;
    Render::calculateRenderPosition(sprite, isSVG);

    params.centered = true;
    params.x = sprite->renderInfo.renderX;
    params.y = sprite->renderInfo.renderY;
    params.scale = sprite->renderInfo.renderScaleY;
    params.rotation = sprite->renderInfo.renderRotation;
    params.flip = (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0);
    params.opacity = 1.0f - (std::clamp(sprite->ghostEffect, 0.0f, 100.0f) * 0.01f);
    params.brightness = sprite->brightnessEffect;

    image->render(params, penLayer);
}

void Renderer_GDI::penClear() {
    PatBlt(penLayer, 0, 0, this->getWidth(), this->getHeight(), BLACKNESS);
    PatBlt(penLayerMask, 0, 0, this->getWidth(), this->getHeight(), BLACKNESS);
}

void Renderer_GDI::beginFrame(int screen, int colorR, int colorG, int colorB) {
    if (!hasFrameBegan) {
        HBRUSH brush = CreateSolidBrush(RGB(colorR, colorG, colorB));
        RECT rc;
        WindowWin32 *win = (WindowWin32 *)window;

        rc.left = 0;
        rc.top = 0;
        rc.right = this->getWidth();
        rc.bottom = this->getHeight();

        FillRect(renderer, &rc, brush);
        DeleteObject(brush);

        if (win->resized) {
            resized(rc.right, rc.bottom);
            win->resized = false;
        }

        hasFrameBegan = true;
    }
}

void Renderer_GDI::endFrame(bool shouldFlush) {
    Sleep(16);
    if (this->window) this->window->swapBuffers();
    hasFrameBegan = false;
}

void Renderer_GDI::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    HBRUSH brush = CreateSolidBrush(RGB(colorR, colorG, colorB));
    RECT rc;

    rc.left = x - w / 2;
    rc.top = y - h / 2;
    rc.right = x + w / 2;
    rc.bottom = y + h / 2;

    FillRect(renderer, &rc, brush);
    DeleteObject(brush);
}

void Renderer_GDI::drawRoundedBox(int w, int h, int x, int y, int radius, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    radius = std::clamp(radius, 0, std::min(w, h) / 2);
    if (radius <= 0) {
        drawBox(w, h, x, y, colorR, colorG, colorB, colorA);
        return;
    }

    HBRUSH brush = CreateSolidBrush(RGB(colorR, colorG, colorB));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(colorR, colorG, colorB));
    HGDIOBJ oldBrush = SelectObject(renderer, brush);
    HGDIOBJ oldPen = SelectObject(renderer, pen);

    RoundRect(renderer, x - w / 2, y - h / 2, x + w / 2, y + h / 2, radius * 2, radius * 2);

    SelectObject(renderer, oldBrush);
    SelectObject(renderer, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void Renderer_GDI::renderPenLayer() {
    POINT p[3];
    int i;

    p[0].x = 0;
    p[0].y = 0;
    p[1].x = this->getWidth();
    p[1].y = 0;
    p[2].x = 0;
    p[2].y = this->getHeight();

    for (i = 0; i < penWidth * penHeight; i++) {
        if (penLayerMaskQuad[i].rgbRed) {
            penLayerQuad[i].rgbReserved = 0xff;
        }
    }

    Image_GDI::PlgAlphaBlt(renderer, p, penLayer, 0, 0, penWidth, penHeight, 255);
}

void Renderer_GDI::renderSprites() {
    this->beginFrame(0, 255, 255, 255);

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

        if (currentSprite->isStage) renderPenLayer();
    }

    if (speechManager) {
        speechManager->render();
    }

    Render::renderMonitors();

    drawBlackBars(this->getWidth(), this->getHeight());
    this->endFrame(true);
}

bool Renderer_GDI::appShouldRun() {
    if (OS::toExit) return false;
    if (this->window) {
        this->window->pollEvents();
        return !this->window->shouldClose();
    }
    return false;
}
