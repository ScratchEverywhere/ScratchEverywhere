#include "renderer_sdl3.hpp"
#include "speech_manager_sdl3.hpp"
#include "window_sdl3.hpp"
#include <window.hpp>

#include <algorithm>
#include <audio.hpp>
#include <cmath>
#include <cstdlib>
#include <downloader.hpp>
#include <image.hpp>
#include <image_sdl3.hpp>
#include <input.hpp>
#include <log.hpp>
#include <math.hpp>
#include <runtime.hpp>
#include <string>
#include <text.hpp>
#include <types.hpp>
#include <unordered_map>
#include <vector>

#ifdef __SWITCH__
#include <switch.h>

char nickname[0x21];
#endif

#ifdef VITA
#include <psp2/io/fcntl.h>
#include <psp2/net/http.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <psp2/touch.h>
#endif

static SDL_Texture *mainRenderTarget = nullptr;
static bool hasOverrideTarget = false;
static int overrideWidth = 0;
static int overrideHeight = 0;
SDL_Texture *penTexture = nullptr;

static std::vector<SDL_Vertex> penVerts;

#ifdef __XBOX__
static void flushPenVertsBatch(SDL_Renderer *renderer, SDL_Texture *penTex, SDL_Texture *mainTarget) {
    if (penVerts.empty() || !penTex) return;
    SDL_SetRenderTarget(renderer, penTex);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    constexpr size_t BATCH_SIZE = 2048 * 3; // 6144 vertices
    for (size_t i = 0; i < penVerts.size(); i += BATCH_SIZE) {
        size_t count = std::min(BATCH_SIZE, penVerts.size() - i);
        SDL_RenderGeometry(renderer, nullptr, &penVerts[i], static_cast<int>(count), nullptr, 0);
        SDL_FlushRenderer(renderer);
    }
    penVerts.clear();
    SDL_SetRenderTarget(renderer, mainTarget);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}
#else
#endif

void drawBlackBars(SDL_Renderer *renderer, int screenWidth, int screenHeight) {
    float screenAspect = static_cast<float>(screenWidth) / screenHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;

    if (screenAspect > projectAspect) {
        // Vertical bars
        float scale = static_cast<float>(screenHeight) / Scratch::projectHeight;
        float scaledProjectWidth = Scratch::projectWidth * scale;
        float barWidth = (screenWidth - scaledProjectWidth) / 2.0f;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_FRect leftBar = {0, 0, std::ceil(barWidth), static_cast<float>(screenHeight)};
        SDL_FRect rightBar = {std::floor(screenWidth - barWidth), 0, std::ceil(barWidth), static_cast<float>(screenHeight)};

        SDL_RenderFillRect(renderer, &leftBar);
        SDL_RenderFillRect(renderer, &rightBar);
    } else if (screenAspect < projectAspect) {
        // Horizontal bars
        float scale = static_cast<float>(screenWidth) / Scratch::projectWidth;
        float scaledProjectHeight = Scratch::projectHeight * scale;
        float barHeight = (screenHeight - scaledProjectHeight) / 2.0f;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_FRect topBar = {0, 0, static_cast<float>(screenWidth), std::ceil(barHeight)};
        SDL_FRect bottomBar = {0, std::floor(screenHeight - barHeight), static_cast<float>(screenWidth), std::ceil(barHeight)};

        SDL_RenderFillRect(renderer, &topBar);
        SDL_RenderFillRect(renderer, &bottomBar);
    }
}

Renderer_SDL3::Renderer_SDL3() {
#ifdef __SWITCH__
    int windowWidth = 1280;
    int windowHeight = 720;
#elif defined(VITA)
    int windowWidth = 960;
    int windowHeight = 544;
#elif defined(__XBOX__)
    int windowWidth = 640;
    int windowHeight = 480;
#else
    int windowWidth = 480;
    int windowHeight = 360;
#endif

    window = createWindow(windowWidth, windowHeight, "Scratch Everywhere!", WindowingBackends::SDL3);
    if (window->error.has_value()) {
        error = "Failed to init window: " + window->error.value();
        delete window;
        window = nullptr;
        return;
    }
#ifdef __XBOX__
    Sdlrenderer = static_cast<SDL_Renderer *>(static_cast<WindowSDL3 *>(window)->getRenderer());
#else
    Sdlrenderer = SDL_CreateRenderer((SDL_Window *)window->getHandle(), "");
#endif
    if (Sdlrenderer == nullptr) {
        error = "Could not create renderer: " + std::string(SDL_GetError());
        return;
    }
}

Renderer_SDL3::~Renderer_SDL3() {
    if (speechManager) {
        delete speechManager;
        speechManager = nullptr;
    }
    if (penTexture != nullptr) SDL_DestroyTexture(penTexture);

    TextObject::cleanupText();
    SDL_DestroyRenderer(Sdlrenderer);

    if (window) {
        window->cleanup();
        delete window;
        window = nullptr;
    }

    SDL_Quit();
}

void Renderer_SDL3::setRenderTarget(void *renderTarget, int width, int height) {
    mainRenderTarget = static_cast<SDL_Texture *>(renderTarget);
    overrideWidth = width;
    overrideHeight = height;
    hasOverrideTarget = true;
    SDL_SetRenderTarget(Sdlrenderer, mainRenderTarget);
}

void Renderer_SDL3::clearRenderTarget() {
    mainRenderTarget = nullptr;
    hasOverrideTarget = false;
    SDL_SetRenderTarget(Sdlrenderer, mainRenderTarget);
}

bool Renderer_SDL3::createSpeechManager() {
    if (speechManager == nullptr) speechManager = new SpeechManagerSDL3();
    return speechManager != nullptr;
}

void Renderer_SDL3::destroySpeechManager() {
    delete speechManager;
    speechManager = nullptr;
}

SpeechManager *Renderer_SDL3::getSpeechManager() {
    if (speechManager == nullptr) speechManager = new SpeechManagerSDL3();
    return speechManager;
}

int Renderer_SDL3::getWidth() {
    if (hasOverrideTarget) return overrideWidth;
    if (window) return window->getWidth();
    return 480;
}

int Renderer_SDL3::getHeight() {
    if (hasOverrideTarget) return overrideHeight;
    if (window) return window->getHeight();
    return 360;
}

float Renderer_SDL3::getPixelDensity() {
    if (window) return window->getPixelDensity();
    return 1.0f;
}

bool Renderer_SDL3::initPen() {
    if (penTexture != nullptr) return true;

#if defined(__XBOX__)
    constexpr auto penFormat = SDL_PIXELFORMAT_ARGB8888;
#else
    constexpr auto penFormat = SDL_PIXELFORMAT_RGBA8888;
#endif

    if (Scratch::hqpen) {
        if (Scratch::projectWidth / static_cast<double>(getWidth()) < Scratch::projectHeight / static_cast<double>(getHeight()))
            penTexture = SDL_CreateTexture(Sdlrenderer, penFormat, SDL_TEXTUREACCESS_TARGET, Scratch::projectWidth * (getHeight() / static_cast<double>(Scratch::projectHeight)), getHeight());
        else
            penTexture = SDL_CreateTexture(Sdlrenderer, penFormat, SDL_TEXTUREACCESS_TARGET, getWidth(), Scratch::projectHeight * (getWidth() / static_cast<double>(Scratch::projectWidth)));
    } else penTexture = SDL_CreateTexture(Sdlrenderer, penFormat, SDL_TEXTUREACCESS_TARGET, Scratch::projectWidth, Scratch::projectHeight);

#ifdef __XBOX__
    if (!penTexture) {
        Log::logError(std::string("Failed to create pen texture: ") + SDL_GetError());
        return false;
    }
#else
#endif

    // Clear the texture
    SDL_SetTextureBlendMode(penTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(penTexture, SDL_SCALEMODE_NEAREST);
    SDL_SetRenderTarget(Sdlrenderer, penTexture);
    SDL_SetRenderDrawColor(Sdlrenderer, 0, 0, 0, 0);
    SDL_RenderClear(Sdlrenderer);
    SDL_SetRenderTarget(Sdlrenderer, mainRenderTarget);

    return true;
}

void Renderer_SDL3::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
#ifdef __XBOX__
    if (!penTexture) return;
#else
#endif
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    const float alpha = (100.0f - sprite->penData.color.transparency) / 100.0f;

    float penWidth = 640.0f;
    float penHeight = 480.0f;
    SDL_GetTextureSize(penTexture, &penWidth, &penHeight);

    const double scale = (penHeight / static_cast<double>(Scratch::projectHeight));

    const float sx1 = static_cast<float>(x1 * scale + penWidth / 2.0);
    const float sy1 = static_cast<float>(-y1 * scale + penHeight / 2.0);
    const float sx2 = static_cast<float>(x2 * scale + penWidth / 2.0);
    const float sy2 = static_cast<float>(-y2 * scale + penHeight / 2.0);

    const double dx = sx2 - sx1;
    const double dy = sy2 - sy1;

    const double length = sqrt(dx * dx + dy * dy);
    const double drawWidth = (sprite->penData.size / 2.0f) * scale;

    const SDL_FColor sdlColor = {
        rgbColor.r / 255.0f,
        rgbColor.g / 255.0f,
        rgbColor.b / 255.0f,
        alpha};

    if (length > 0) {
        const float nx = static_cast<float>((-dy / length) * drawWidth);
        const float ny = static_cast<float>((dx / length) * drawWidth);

        const SDL_Vertex v0 = {{sx1 + nx, sy1 + ny}, sdlColor, {0.0f, 0.0f}};
        const SDL_Vertex v1 = {{sx1 - nx, sy1 - ny}, sdlColor, {0.0f, 0.0f}};
        const SDL_Vertex v2 = {{sx2 + nx, sy2 + ny}, sdlColor, {0.0f, 0.0f}};
        const SDL_Vertex v3 = {{sx2 - nx, sy2 - ny}, sdlColor, {0.0f, 0.0f}};

        penVerts.push_back(v0);
        penVerts.push_back(v1);
        penVerts.push_back(v2);

        penVerts.push_back(v1);
        penVerts.push_back(v3);
        penVerts.push_back(v2);
    }

    const unsigned int circleSegments = std::max(8.0, 8.0f * (sprite->penData.size / 150.0f));
    const double angleStep = 2.0 * M_PI / circleSegments;

    for (int i = 0; i < circleSegments; ++i) {
        const double angle1 = i * angleStep;
        const double angle2 = (i + 1) * angleStep;

        float x1_c = sx1 + static_cast<float>(cos(angle1) * drawWidth);
        float y1_c = sy1 + static_cast<float>(sin(angle1) * drawWidth);
        float x2_c = sx1 + static_cast<float>(cos(angle2) * drawWidth);
        float y2_c = sy1 + static_cast<float>(sin(angle2) * drawWidth);

        const SDL_Vertex cv0 = {{sx1, sy1}, sdlColor, {0.0f, 0.0f}};
        const SDL_Vertex cv1 = {{x1_c, y1_c}, sdlColor, {0.0f, 0.0f}};
        const SDL_Vertex cv2 = {{x2_c, y2_c}, sdlColor, {0.0f, 0.0f}};

        penVerts.push_back(cv0);
        penVerts.push_back(cv1);
        penVerts.push_back(cv2);

        x1_c = sx2 + static_cast<float>(cos(angle1) * drawWidth);
        y1_c = sy2 + static_cast<float>(sin(angle1) * drawWidth);
        x2_c = sx2 + static_cast<float>(cos(angle2) * drawWidth);
        y2_c = sy2 + static_cast<float>(sin(angle2) * drawWidth);

        const SDL_Vertex cv3 = {{sx2, sy2}, sdlColor, {0.0f, 0.0f}};
        const SDL_Vertex cv4 = {{x1_c, y1_c}, sdlColor, {0.0f, 0.0f}};
        const SDL_Vertex cv5 = {{x2_c, y2_c}, sdlColor, {0.0f, 0.0f}};

        penVerts.push_back(cv3);
        penVerts.push_back(cv4);
        penVerts.push_back(cv5);
    }
#ifdef __XBOX__
    if (penVerts.size() >= 6144) {
        flushPenVertsBatch(Sdlrenderer, penTexture, mainRenderTarget);
    }
#else
#endif
}

void Renderer_SDL3::penDotAccurate(Sprite *sprite) {
#ifdef __XBOX__
    if (!penTexture) return;
#else
#endif
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    const float alpha = (100.0f - sprite->penData.color.transparency) / 100.0f;

    float penWidth = 640.0f;
    float penHeight = 480.0f;
    SDL_GetTextureSize(penTexture, &penWidth, &penHeight);

    const double scale = (penHeight / static_cast<double>(Scratch::projectHeight));

    const float sx = static_cast<float>(sprite->xPosition * scale + penWidth / 2.0);
    const float sy = static_cast<float>(-sprite->yPosition * scale + penHeight / 2.0);

    const float radius = static_cast<float>((sprite->penData.size / 2.0f) * scale);

    const SDL_FColor sdlColor = {
        rgbColor.r / 255.0f,
        rgbColor.g / 255.0f,
        rgbColor.b / 255.0f,
        alpha};

    const unsigned int circleSegments = std::max(16.0, 16.0f * (sprite->penData.size / 150.0f));
    const double angleStep = 2.0 * M_PI / circleSegments;

    for (int i = 0; i < circleSegments; ++i) {
        const double angle1 = i * angleStep;
        const double angle2 = (i + 1) * angleStep;

        const float x1 = sx + static_cast<float>(cos(angle1) * radius);
        const float y1 = sy + static_cast<float>(sin(angle1) * radius);
        const float x2 = sx + static_cast<float>(cos(angle2) * radius);
        const float y2 = sy + static_cast<float>(sin(angle2) * radius);

        const SDL_Vertex v0 = {{sx, sy}, sdlColor, {0.0f, 0.0f}};
        const SDL_Vertex v1 = {{x1, y1}, sdlColor, {0.0f, 0.0f}};
        const SDL_Vertex v2 = {{x2, y2}, sdlColor, {0.0f, 0.0f}};

        penVerts.push_back(v0);
        penVerts.push_back(v1);
        penVerts.push_back(v2);
    }
#ifdef __XBOX__
    if (penVerts.size() >= 6144) {
        flushPenVertsBatch(Sdlrenderer, penTexture, mainRenderTarget);
    }
#else
#endif
}

void Renderer_SDL3::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
#ifdef __XBOX__
    if (!penTexture) return;
#else
#endif
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    float alpha = (100.0 - sprite->penData.color.transparency) / 100.0;

    float penWidth = 640;
    float penHeight = 480;
    SDL_GetTextureSize(penTexture, &penWidth, &penHeight);

    const double scale = (penHeight / static_cast<double>(Scratch::projectHeight));

    const float sx1 = static_cast<float>(x1 * scale + penWidth / 2.0);
    const float sy1 = static_cast<float>(-y1 * scale + penHeight / 2.0);
    const float sx2 = static_cast<float>(x2 * scale + penWidth / 2.0);
    const float sy2 = static_cast<float>(-y2 * scale + penHeight / 2.0);

    const double dx = sx2 - sx1;
    const double dy = sy2 - sy1;

    const double length = sqrt(dx * dx + dy * dy);
    const double drawWidth = (sprite->penData.size / 2.0f) * scale;

    if (length <= 0) return;

    const float nx = static_cast<float>((-dy / length) * drawWidth);
    const float ny = static_cast<float>((dx / length) * drawWidth);

    const SDL_FColor sdlColor = {
        static_cast<float>(rgbColor.r / 255.0f),
        static_cast<float>(rgbColor.g / 255.0f),
        static_cast<float>(rgbColor.b / 255.0f),
        static_cast<float>(alpha)};

    const SDL_Vertex v0 = {{sx1 + nx, sy1 + ny}, sdlColor, {0.0f, 0.0f}}; // top left
    const SDL_Vertex v1 = {{sx1 - nx, sy1 - ny}, sdlColor, {0.0f, 0.0f}}; // bottom left
    const SDL_Vertex v2 = {{sx2 + nx, sy2 + ny}, sdlColor, {0.0f, 0.0f}}; // top right
    const SDL_Vertex v3 = {{sx2 - nx, sy2 - ny}, sdlColor, {0.0f, 0.0f}}; // bottom right

    // squarey
    penVerts.push_back(v0);
    penVerts.push_back(v1);
    penVerts.push_back(v2);

    penVerts.push_back(v1);
    penVerts.push_back(v3);
    penVerts.push_back(v2);
#ifdef __XBOX__
    if (penVerts.size() >= 6144) {
        flushPenVertsBatch(Sdlrenderer, penTexture, mainRenderTarget);
    }
#else
#endif
}

void Renderer_SDL3::penDotFast(Sprite *sprite) {
#ifdef __XBOX__
    if (!penTexture) return;
#else
#endif
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    float alpha = (100.0 - sprite->penData.color.transparency) / 100.0;

    float penWidth = 640;
    float penHeight = 480;
    SDL_GetTextureSize(penTexture, &penWidth, &penHeight);

    const double scale = (penHeight / static_cast<double>(Scratch::projectHeight));

    const float sx = static_cast<float>(sprite->xPosition * scale + penWidth / 2.0);
    const float sy = static_cast<float>(-sprite->yPosition * scale + penHeight / 2.0);

    const float halfSize = static_cast<float>((sprite->penData.size / 2.0f) * scale);

    const SDL_FColor sdlColor = {
        static_cast<float>(rgbColor.r / 255.0f),
        static_cast<float>(rgbColor.g / 255.0f),
        static_cast<float>(rgbColor.b / 255.0f),
        alpha};

    const SDL_Vertex v0 = {{sx - halfSize, sy - halfSize}, sdlColor, {0.0f, 0.0f}}; // top left
    const SDL_Vertex v1 = {{sx - halfSize, sy + halfSize}, sdlColor, {0.0f, 0.0f}}; // bottom left
    const SDL_Vertex v2 = {{sx + halfSize, sy - halfSize}, sdlColor, {0.0f, 0.0f}}; // top right
    const SDL_Vertex v3 = {{sx + halfSize, sy + halfSize}, sdlColor, {0.0f, 0.0f}}; // bottom right

    // squarey
    penVerts.push_back(v0);
    penVerts.push_back(v1);
    penVerts.push_back(v2);

    penVerts.push_back(v1);
    penVerts.push_back(v3);
    penVerts.push_back(v2);
#ifdef __XBOX__
    if (penVerts.size() >= 6144) {
        flushPenVertsBatch(Sdlrenderer, penTexture, mainRenderTarget);
    }
#else
#endif
}

void Renderer_SDL3::penStamp(Sprite *sprite) {
#ifdef __XBOX__
    if (!penTexture) return;
    flushPenVertsBatch(Sdlrenderer, penTexture, mainRenderTarget);
#else
#endif
    auto imgFind = Scratch::costumeImages.find(sprite->costumes[sprite->currentCostume].fullName);
    if (imgFind == Scratch::costumeImages.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return;
    }

    const Costume &costume = sprite->costumes[sprite->currentCostume];

    SDL_SetRenderTarget(Sdlrenderer, penTexture);

    // clear line draw queue so stamp can be rendered on top
#ifdef __XBOX__
#else
    if (!penVerts.empty()) {
        SDL_SetRenderDrawBlendMode(Sdlrenderer, SDL_BLENDMODE_BLEND);
        SDL_RenderGeometry(Sdlrenderer, nullptr, penVerts.data(), penVerts.size(), nullptr, 0);
        SDL_SetRenderDrawBlendMode(Sdlrenderer, SDL_BLENDMODE_NONE);
        penVerts.clear();
    }
#endif

    Image_SDL3 *image = reinterpret_cast<Image_SDL3 *>(imgFind->second.get());

    const bool isSVG = costume.isSVG;
    Render::calculateRenderPosition(sprite, isSVG);

    // Pen mapping stuff
    const auto &cords = Scratch::screenToScratchCoords(sprite->renderInfo.renderX, sprite->renderInfo.renderY, getWidth(), getHeight());
    int penX = cords.first + Scratch::projectWidth / 2;
    int penY = -cords.second + Scratch::projectHeight / 2;

    float penScale;
    if (Scratch::hqpen) {
        float penWidth;
        float penHeight;
#ifdef __XBOX__
        SDL_GetTextureSize(penTexture, &penWidth, &penHeight);
#else
        SDL_GetTextureSize(image->texture, &penWidth, &penHeight);
#endif
        const double scale = (penHeight / static_cast<double>(Scratch::projectHeight));

        penX *= scale;
        penY *= scale;
        penScale = sprite->renderInfo.renderScaleY;
    } else {
#ifdef __XBOX__
        penScale = (sprite->size / 100.0f) / costume.bitmapResolution;
        if (Scratch::bitmapHalfQuality && !costume.isSVG && costume.bitmapResolution == 2) {
            penScale *= 2.0f;
        }
#else
        penScale = (sprite->size / 100.0f) / costume.bitmapResolution;
#endif
    }

    ImageRenderParams params;
    params.centered = true;
    params.x = penX;
    params.y = penY;
    params.rotation = sprite->renderInfo.renderRotation;
    params.scale = penScale;
    params.flip = (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0);
    params.opacity = 1.0f - (std::clamp(sprite->ghostEffect, 0.0f, 100.0f) * 0.01f);
    params.brightness = sprite->brightnessEffect;

    image->render(params);

    SDL_SetRenderTarget(Sdlrenderer, mainRenderTarget);
}

void Renderer_SDL3::penClear() {
    if (!penTexture || penTexture == nullptr) return;
    SDL_SetRenderTarget(Sdlrenderer, penTexture);
    SDL_SetRenderDrawColor(Sdlrenderer, 0, 0, 0, 0);
    SDL_RenderClear(Sdlrenderer);
    SDL_SetRenderTarget(Sdlrenderer, mainRenderTarget);
    penVerts.clear();
}

void Renderer_SDL3::cleanupPen() {
    if (penTexture != nullptr) {
        SDL_DestroyTexture(penTexture);
        penTexture = nullptr;
    }
    penVerts.clear();
    penVerts.shrink_to_fit();
}

void Renderer_SDL3::beginFrame(int screen, int colorR, int colorG, int colorB) {
    if (!hasFrameBegan) {
        SDL_SetRenderDrawColor(Sdlrenderer, colorR, colorG, colorB, 255);
        SDL_RenderClear(Sdlrenderer);
        hasFrameBegan = true;
    }
}

void Renderer_SDL3::endFrame(bool shouldFlush) {
    SDL_RenderPresent(Sdlrenderer);
    SDL_Delay(16);
    hasFrameBegan = false;
}

void Renderer_SDL3::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    SDL_SetRenderDrawColor(Sdlrenderer, colorR, colorG, colorB, colorA);
    SDL_FRect rect = {x - (w / 2.0f), y - (h / 2.0f), static_cast<float>(w), static_cast<float>(h)};
    SDL_RenderFillRect(Sdlrenderer, &rect);
}

void Renderer_SDL3::drawRoundedBox(int w, int h, int x, int y, int radius, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    radius = std::clamp(radius, 0, std::min(w, h) / 2);
    if (radius <= 0) {
        drawBox(w, h, x, y, colorR, colorG, colorB, colorA);
        return;
    }

    const SDL_FColor col = {colorR / 255.0f, colorG / 255.0f, colorB / 255.0f, colorA / 255.0f};
    const float left = x - w / 2.0f, right = x + w / 2.0f;
    const float top = y - h / 2.0f, bottom = y + h / 2.0f;
    const float r = static_cast<float>(radius);

    constexpr int cornerSegments = 4;
    std::vector<SDL_FPoint> outline;
    outline.reserve((cornerSegments + 1) * 4);

    auto addArc = [&](float cx, float cy, float startAngle) {
        for (int i = 0; i <= cornerSegments; ++i) {
            const float a = startAngle + (static_cast<float>(M_PI) / 2.0f) * (i / static_cast<float>(cornerSegments));
            outline.push_back({cx + std::cos(a) * r, cy + std::sin(a) * r});
        }
    };

    addArc(right - r, top + r, -static_cast<float>(M_PI) / 2.0f);
    addArc(right - r, bottom - r, 0.0f);
    addArc(left + r, bottom - r, static_cast<float>(M_PI) / 2.0f);
    addArc(left + r, top + r, static_cast<float>(M_PI));

    std::vector<SDL_Vertex> verts;
    verts.reserve((outline.size() - 2) * 3);
    for (size_t i = 1; i + 1 < outline.size(); ++i) {
        verts.push_back({outline[0], col, {0.0f, 0.0f}});
        verts.push_back({outline[i], col, {0.0f, 0.0f}});
        verts.push_back({outline[i + 1], col, {0.0f, 0.0f}});
    }

    SDL_RenderGeometry(Sdlrenderer, nullptr, verts.data(), static_cast<int>(verts.size()), nullptr, 0);
}

void Renderer_SDL3::renderSprites() {
    SDL_SetRenderDrawColor(Sdlrenderer, 255, 255, 255, 255);
    SDL_RenderClear(Sdlrenderer);

    for (auto it = Scratch::sprites.rbegin(); it != Scratch::sprites.rend(); ++it) {
        Sprite *currentSprite = *it;
        auto imgFind = Scratch::costumeImages.find(currentSprite->costumes[currentSprite->currentCostume].fullName);
        if (imgFind != Scratch::costumeImages.end()) {
            Image *image = imgFind->second.get();

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

        if (currentSprite->isStage) renderPenLayer();
    }

    if (speechManager) {
        speechManager->render();
    }

    drawBlackBars(Sdlrenderer, getWidth(), getHeight());
    Render::renderMonitors();

#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
    if (Input::mousePointer.isMoving) {
        SDL_SetRenderDrawColor(Sdlrenderer, 0, 0, 255, 255);
        SDL_FRect rect;
        rect.w = rect.h = 5;
        rect.x = (Input::mousePointer.x * Render::renderScale) + (window->getWidth() * 0.5);
        rect.y = (Input::mousePointer.y * -1 * Render::renderScale) + (window->getHeight() * 0.5);
        Input::mousePointer.x = std::clamp((float)Input::mousePointer.x, -Scratch::projectWidth * 0.5f, Scratch::projectWidth * 0.5f);
        Input::mousePointer.y = std::clamp((float)Input::mousePointer.y, -Scratch::projectHeight * 0.5f, Scratch::projectHeight * 0.5f);
        SDL_RenderRect(Sdlrenderer, &rect);
    }
#endif

    SDL_RenderPresent(Sdlrenderer);
}

void Renderer_SDL3::renderPenLayer() {
#ifdef __XBOX__
    if (!penTexture) return;
    flushPenVertsBatch(Sdlrenderer, penTexture, mainRenderTarget);
#else
    if (!penVerts.empty()) {
        SDL_SetRenderTarget(Sdlrenderer, penTexture);
        SDL_SetRenderDrawBlendMode(Sdlrenderer, SDL_BLENDMODE_BLEND);

        SDL_RenderGeometry(Sdlrenderer, nullptr, penVerts.data(), penVerts.size(), nullptr, 0);
        penVerts.clear();

        SDL_SetRenderTarget(Sdlrenderer, mainRenderTarget);
        SDL_SetRenderDrawBlendMode(Sdlrenderer, SDL_BLENDMODE_NONE);
    }
#endif

    SDL_FRect renderRect = {0, 0, 0, 0};

    if (static_cast<float>(getWidth()) / getHeight() > static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight) {
        renderRect.x = std::ceil((getWidth() - Scratch::projectWidth * (static_cast<float>(getHeight()) / Scratch::projectHeight)) / 2.0f);
        renderRect.w = getWidth() - renderRect.x * 2;
        renderRect.h = getHeight();
    } else {
        renderRect.y = std::ceil((getHeight() - Scratch::projectHeight * (static_cast<float>(getWidth()) / Scratch::projectWidth)) / 2.0f);
        renderRect.h = getHeight() - renderRect.y * 2;
        renderRect.w = getWidth();
    }

    SDL_RenderTexture(Sdlrenderer, penTexture, nullptr, &renderRect);
}

bool Renderer_SDL3::appShouldRun() {
    if (OS::toExit) return false;
    if (window) {
        window->pollEvents();

        static int lastW = 0, lastH = 0;
        int currentW = getWidth();
        int currentH = getHeight();

        if (lastW != currentW || lastH != currentH) {
            lastW = currentW;
            lastH = currentH;

            if (Scratch::hqpen) {
                // Recreate pen texture
                SDL_DestroyTexture(penTexture);
                penTexture = nullptr;
                initPen();
            }
        }

        return !window->shouldClose();
    }
    return false;
}
