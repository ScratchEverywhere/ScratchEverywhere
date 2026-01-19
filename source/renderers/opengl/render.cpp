#include "render.hpp"
#include "image.hpp"
#include "speech_manager_gl.hpp"
#include <window.hpp>
#if defined(WINDOWING_GLFW)
#include <windowing/glfw/window.hpp>
#elif defined(WINDOWING_SDL1)
#include <windowing/sdl1/window.hpp>
#elif defined(WINDOWING_SDL2)
#include <windowing/sdl2/window.hpp>
#elif defined(WINDOWING_SDL3)
#include <windowing/sdl3/window.hpp>
#else
#error "No windowing backend defined"
#endif
#include <algorithm>
#include <audio.hpp>
#include <chrono>
#include <cmath>
#include <color.hpp>
#include <cstdlib>
#include <downloader.hpp>
#include <image.hpp>
#include <math.hpp>
#include <render.hpp>
#include <runtime.hpp>
#include <sprite.hpp>
#include <string>
#include <unordered_map>
#include <unzip.hpp>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Window *globalWindow = nullptr;

Render::RenderModes Render::renderMode = Render::TOP_SCREEN_ONLY;
bool Render::hasFrameBegan;
std::vector<Monitor> Render::visibleVariables;
std::unordered_map<std::string, std::pair<std::unique_ptr<TextObject>, std::unique_ptr<TextObject>>> Render::monitorTexts;
std::unordered_map<std::string, Render::ListMonitorRenderObjects> Render::listMonitors;
std::chrono::system_clock::time_point Render::startTime = std::chrono::system_clock::now();
std::chrono::system_clock::time_point Render::endTime = std::chrono::system_clock::now();
bool Render::debugMode = false;
float Render::renderScale = 1.0f;

SpeechManagerGL *speechManager = nullptr;

static unsigned int penTexture = 0;
static int penWidth = 0;
static int penHeight = 0;

bool Render::Init() {
#if defined(WINDOWING_GLFW)
    globalWindow = new WindowGLFW();
#elif defined(WINDOWING_SDL1)
    globalWindow = new WindowSDL1();
#elif defined(WINDOWING_SDL2)
    globalWindow = new WindowSDL2();
#elif defined(WINDOWING_SDL3)
    globalWindow = new WindowSDL3();
#else
#error "No windowing backend defined"
#endif

    if (!globalWindow->init(540, 405, "Scratch Everywhere!")) {
        delete globalWindow;
        globalWindow = nullptr;
        return false;
    }

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glViewport(0, 0, getWidth(), getHeight());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, getWidth(), getHeight(), 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    setRenderScale();

    debugMode = true;

    return true;
}

void Render::deInit() {
    if (penTexture != 0) {
        glDeleteTextures(1, &penTexture);
        penTexture = 0;
    }
    Image::cleanupImages();
    SoundPlayer::cleanupAudio();
    TextObject::cleanupText();

    if (globalWindow) {
        globalWindow->cleanup();
        delete globalWindow;
        globalWindow = nullptr;
    }
}

void *Render::getRenderer() {
    return nullptr;
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

bool Render::initPen() {
    if (penTexture != 0) return true;

    if (Scratch::hqpen) {
        if (Scratch::projectWidth / static_cast<double>(getWidth()) < Scratch::projectHeight / static_cast<double>(getHeight())) {
            penWidth = Scratch::projectWidth * (getHeight() / static_cast<double>(Scratch::projectHeight));
            penHeight = getHeight();
        } else {
            penWidth = getWidth();
            penHeight = Scratch::projectHeight * (getWidth() / static_cast<double>(Scratch::projectWidth));
        }
    } else {
        penWidth = Scratch::projectWidth;
        penHeight = Scratch::projectHeight;
    }

    glGenTextures(1, &penTexture);
    glBindTexture(GL_TEXTURE_2D, penTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    std::vector<uint8_t> emptyData(penWidth * penHeight * 4, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, penWidth, penHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, emptyData.data());

    return true;
}

void Render::penClear() {
    std::vector<uint8_t> emptyData(penWidth * penHeight * 4, 0);
    glBindTexture(GL_TEXTURE_2D, penTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, penWidth, penHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, emptyData.data());
}

static void drawCircle(float x, float y, float radius, ColorRGBA color, uint8_t alpha) {
    int segments = 20;
    glBegin(GL_TRIANGLE_FAN);
    glColor4ub((uint8_t)color.r, (uint8_t)color.g, (uint8_t)color.b, alpha);
    glVertex2f(x, y);
    for (int i = 0; i <= segments; i++) {
        float angle = i * 2.0f * M_PI / segments;
        glVertex2f(x + std::cos(angle) * radius, y + std::sin(angle) * radius);
    }
    glEnd();
}

static void preparePenDrawing() {
    glPushAttrib(GL_VIEWPORT_BIT | GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT);

    glViewport(0, 0, penWidth, penHeight);
    glScissor(0, 0, penWidth, penHeight);
    glEnable(GL_SCISSOR_TEST);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, penWidth, penHeight, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, penTexture);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(penWidth, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(penWidth, penHeight);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(0.0f, penHeight);
    glEnd();
}

static void finishPenDrawing() {
    glBindTexture(GL_TEXTURE_2D, penTexture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, penWidth, penHeight);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glPopAttrib();
}

void Render::penMove(double x1, double y1, double x2, double y2, Sprite *sprite) {
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    uint8_t alpha = (uint8_t)((100.0f - sprite->penData.color.transparency) / 100.0f * 255.0f);

    preparePenDrawing();

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const double scale = (penHeight / static_cast<double>(Scratch::projectHeight));
    float px1 = (float)(x1 * scale + penWidth / 2.0f);
    float py1 = (float)(-y1 * scale + penHeight / 2.0f);
    float px2 = (float)(x2 * scale + penWidth / 2.0f);
    float py2 = (float)(-y2 * scale + penHeight / 2.0f);
    float radius = (float)((sprite->penData.size / 2.0f) * scale);

    glColor4ub((uint8_t)rgbColor.r, (uint8_t)rgbColor.g, (uint8_t)rgbColor.b, alpha);

    // Draw line quad
    float dx = px2 - px1;
    float dy = py2 - py1;
    float length = std::sqrt(dx * dx + dy * dy);
    if (length > 0) {
        float nx = -dy / length * radius;
        float ny = dx / length * radius;
        glBegin(GL_QUADS);
        glVertex2f(px1 + nx, py1 + ny);
        glVertex2f(px1 - nx, py1 - ny);
        glVertex2f(px2 - nx, py2 - ny);
        glVertex2f(px2 + nx, py2 + ny);
        glEnd();
    }

    // Draw caps
    drawCircle(px1, py1, radius, rgbColor, alpha);
    drawCircle(px2, py2, radius, rgbColor, alpha);

    finishPenDrawing();
}

void Render::penDot(Sprite *sprite) {
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    uint8_t alpha = (uint8_t)((100.0f - sprite->penData.color.transparency) / 100.0f * 255.0f);

    preparePenDrawing();

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const double scale = (penHeight / static_cast<double>(Scratch::projectHeight));
    float px = (float)(sprite->xPosition * scale + penWidth / 2.0f);
    float py = (float)(-sprite->yPosition * scale + penHeight / 2.0f);
    float radius = (float)((sprite->penData.size / 2.0f) * scale);

    drawCircle(px, py, radius, rgbColor, alpha);

    finishPenDrawing();
}

void Render::penStamp(Sprite *sprite) {
    const auto &imgFind = images.find(sprite->costumes[sprite->currentCostume].id);
    if (imgFind == images.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return;
    }
    imgFind->second.freeTimer = imgFind->second.maxFreeTime;

    preparePenDrawing();
    glEnable(GL_TEXTURE_2D);

    ImageData &image = imgFind->second;
    image.freeTimer = image.maxFreeTime;
    glBindTexture(GL_TEXTURE_2D, image.textureID);
    sprite->rotationCenterX = sprite->costumes[sprite->currentCostume].rotationCenterX;
    sprite->rotationCenterY = sprite->costumes[sprite->currentCostume].rotationCenterY;
    sprite->spriteWidth = image.width >> 1;
    sprite->spriteHeight = image.height >> 1;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const bool isSVG = sprite->costumes[sprite->currentCostume].isSVG;
    Render::calculateRenderPosition(sprite, isSVG);

    auto cords = Scratch::screenToScratchCoords(sprite->renderInfo.renderX, sprite->renderInfo.renderY, getWidth(), getHeight());
    float penX = cords.first + Scratch::projectWidth / 2.0f;
    float penY = -cords.second + Scratch::projectHeight / 2.0f;

    const double scale = (penHeight / static_cast<double>(Scratch::projectHeight));
    if (Scratch::hqpen) {
        penX *= scale;
        penY *= scale;
    }

    float renderScaleX = Scratch::hqpen ? sprite->renderInfo.renderScaleX : sprite->size / (isSVG ? 100.0f : 200.0f);
    float renderScaleY = Scratch::hqpen ? sprite->renderInfo.renderScaleY : sprite->size / (isSVG ? 100.0f : 200.0f);
    if (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0) {
        renderScaleX = -std::abs(renderScaleX);
    } else {
        renderScaleX = std::abs(renderScaleX);
    }

    // set ghost effect
    float ghost = std::clamp(sprite->ghostEffect, 0.0f, 100.0f);
    uint8_t alpha = static_cast<uint8_t>(255 * (1.0f - ghost / 100.0f));
    float brightness = std::clamp(sprite->brightnessEffect, -100.0f, 100.0f) / 100.0f;

    auto drawQuad = [&](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        glColor4ub(r, g, b, a);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(0.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f((float)image.width, 0.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f((float)image.width, (float)image.height);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(0.0f, (float)image.height);
        glEnd();
    };

    glPushMatrix();
    glTranslatef(penX, penY, 0.0f);
    glRotatef(Math::radiansToDegrees(sprite->renderInfo.renderRotation), 0.0f, 0.0f, 1.0f);
    glScalef(renderScaleX, renderScaleY, 1.0f);
    glTranslatef(-image.width / 2.0f, -image.height / 2.0f, 0.0f);

    // set brightness effect
    if (brightness >= 0) {
        drawQuad(255, 255, 255, alpha);
        if (brightness > 0) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            drawQuad(255, 255, 255, (uint8_t)(255 * brightness * (alpha / 255.0f)));
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    } else {
        uint8_t col = (uint8_t)(255 * (1.0f + brightness));
        drawQuad(col, col, col, alpha);
    }

    glPopMatrix();

    finishPenDrawing();
}

void Render::beginFrame(int screen, int colorR, int colorG, int colorB) {
    if (!hasFrameBegan) {
        glViewport(0, 0, getWidth(), getHeight());
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, getWidth(), getHeight(), 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glClearColor(colorR / 255.0f, colorG / 255.0f, colorB / 255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        hasFrameBegan = true;
    }
}

void Render::endFrame(bool shouldFlush) {
    if (globalWindow) globalWindow->swapBuffers();
    if (shouldFlush) Image::FlushImages();
    hasFrameBegan = false;
}

void Render::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    glDisable(GL_TEXTURE_2D);
    glColor4ub(colorR, colorG, colorB, colorA);
    glBegin(GL_QUADS);
    glVertex2f(x - (w / 2.0f), y - (h / 2.0f));
    glVertex2f(x + (w / 2.0f), y - (h / 2.0f));
    glVertex2f(x + (w / 2.0f), y + (h / 2.0f));
    glVertex2f(x - (w / 2.0f), y + (h / 2.0f));
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

void drawBlackBars(int screenWidth, int screenHeight) {
    float screenAspect = static_cast<float>(screenWidth) / screenHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;

    glDisable(GL_TEXTURE_2D);
    glColor4ub(0, 0, 0, 255);

    if (screenAspect > projectAspect) {
        // Screen is wider than project, vertical bars
        float scale = static_cast<float>(screenHeight) / Scratch::projectHeight;
        float scaledProjectWidth = Scratch::projectWidth * scale;
        float barWidth = (screenWidth - scaledProjectWidth) / 2.0f;

        // Left bar
        glBegin(GL_QUADS);
        glVertex2f(0, 0);
        glVertex2f(barWidth, 0);
        glVertex2f(barWidth, (float)screenHeight);
        glVertex2f(0, (float)screenHeight);
        glEnd();

        // Right bar
        glBegin(GL_QUADS);
        glVertex2f(screenWidth - barWidth, 0);
        glVertex2f((float)screenWidth, 0);
        glVertex2f((float)screenWidth, (float)screenHeight);
        glVertex2f(screenWidth - barWidth, (float)screenHeight);
        glEnd();
    } else if (screenAspect < projectAspect) {
        // Screen is taller than project, horizontal bars
        float scale = static_cast<float>(screenWidth) / Scratch::projectWidth;
        float scaledProjectHeight = Scratch::projectHeight * scale;
        float barHeight = (screenHeight - scaledProjectHeight) / 2.0f;

        // Top bar
        glBegin(GL_QUADS);
        glVertex2f(0, 0);
        glVertex2f((float)screenWidth, 0);
        glVertex2f((float)screenWidth, barHeight);
        glVertex2f(0, barHeight);
        glEnd();

        // Bottom bar
        glBegin(GL_QUADS);
        glVertex2f(0, screenHeight - barHeight);
        glVertex2f((float)screenWidth, screenHeight - barHeight);
        glVertex2f((float)screenWidth, (float)screenHeight);
        glVertex2f(0, (float)screenHeight);
        glEnd();
    }
    glEnable(GL_TEXTURE_2D);
}

void Render::renderPenLayer() {
    if (penTexture == 0) return;

    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;
    float windowAspect = static_cast<float>(getWidth()) / getHeight();

    float drawW, drawH, drawX, drawY;

    if (windowAspect > projectAspect) {
        drawH = (float)getHeight();
        drawW = drawH * projectAspect;
        drawX = (getWidth() - drawW) / 2.0f;
        drawY = 0;
    } else {
        drawW = (float)getWidth();
        drawH = drawW / projectAspect;
        drawX = 0;
        drawY = (getHeight() - drawH) / 2.0f;
    }

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, penTexture);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(drawX, drawY);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(drawX + drawW, drawY);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(drawX + drawW, drawY + drawH);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(drawX, drawY + drawH);
    glEnd();
}

void Render::renderSprites() {
    glViewport(0, 0, getWidth(), getHeight());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, getWidth(), getHeight(), 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);

    for (auto it = Scratch::sprites.rbegin(); it != Scratch::sprites.rend(); ++it) {
        Sprite *currentSprite = *it;

        auto imgFind = images.find(currentSprite->costumes[currentSprite->currentCostume].id);
        if (imgFind != images.end()) {
            ImageData &image = imgFind->second;
            glBindTexture(GL_TEXTURE_2D, image.textureID);

            currentSprite->rotationCenterX = currentSprite->costumes[currentSprite->currentCostume].rotationCenterX;
            currentSprite->rotationCenterY = currentSprite->costumes[currentSprite->currentCostume].rotationCenterY;
            currentSprite->spriteWidth = image.width >> 1;
            currentSprite->spriteHeight = image.height >> 1;

            if (!currentSprite->visible) continue;

            image.freeTimer = image.maxFreeTime;

            const bool isSVG = currentSprite->costumes[currentSprite->currentCostume].isSVG;
            calculateRenderPosition(currentSprite, isSVG);
            int renderX = currentSprite->renderInfo.renderX;
            int renderY = currentSprite->renderInfo.renderY;

            float renderScaleX = currentSprite->renderInfo.renderScaleX;
            float renderScaleY = currentSprite->renderInfo.renderScaleY;

            if (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT && currentSprite->rotation < 0) {
                renderScaleX = -std::abs(renderScaleX);
            } else {
                renderScaleX = std::abs(renderScaleX);
            }

            // set ghost effect
            float ghost = std::clamp(currentSprite->ghostEffect, 0.0f, 100.0f);
            uint8_t alpha = static_cast<uint8_t>(255 * (1.0f - ghost / 100.0f));
            float brightness = std::clamp(currentSprite->brightnessEffect, -100.0f, 100.0f) / 100.0f;

            auto drawQuad = [&](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
                glColor4ub(r, g, b, a);
                float w = (float)image.width;
                float h = (float)image.height;
                glBegin(GL_QUADS);
                glTexCoord2f(0.0f, 0.0f);
                glVertex2f(0.0f, 0.0f);
                glTexCoord2f(1.0f, 0.0f);
                glVertex2f(w, 0.0f);
                glTexCoord2f(1.0f, 1.0f);
                glVertex2f(w, h);
                glTexCoord2f(0.0f, 1.0f);
                glVertex2f(0.0f, h);
                glEnd();
            };

            glPushMatrix();
            glTranslatef((float)renderX, (float)renderY, 0.0f);
            glRotatef((float)Math::radiansToDegrees(currentSprite->renderInfo.renderRotation), 0.0f, 0.0f, 1.0f);
            glScalef(renderScaleX, renderScaleY, 1.0f);
            glTranslatef(-image.width / 2.0f, -image.height / 2.0f, 0.0f);

            // set brightness effect
            if (brightness >= 0) {
                drawQuad(255, 255, 255, alpha);
                if (brightness > 0) {
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                    drawQuad(255, 255, 255, (uint8_t)(255 * brightness * (alpha / 255.0f)));
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                }
            } else {
                uint8_t col = (uint8_t)(255 * (1.0f + brightness));
                drawQuad(col, col, col, alpha);
            }

            glPopMatrix();
        }
        if (currentSprite->isStage) renderPenLayer();
    }

    drawBlackBars(getWidth(), getHeight());
    renderVisibleVariables();

    endFrame(true);
}

bool Render::appShouldRun() {
    if (OS::toExit) return false;
    if (globalWindow) {
        globalWindow->pollEvents();
        return !globalWindow->shouldClose();
    }
    return false;
}
