#include "renderer_xbox.hpp"
#include "speech_manager_xbox.hpp"
#include "window_sdl3.hpp"
#include <window.hpp>

#include <algorithm>
#include <audio.hpp>
#include <cmath>
#include <cstdlib>
#include <downloader.hpp>
#include <image.hpp>
#include <image_xbox.hpp>
#include <input.hpp>
#include <log.hpp>
#include <math.hpp>
#include <runtime.hpp>
#include <string>
#include <text.hpp>
#include <types.hpp>
#include <unordered_map>
#include <vector>

static SDL_Texture *mainRenderTarget = nullptr;
static bool hasOverrideTarget = false;
static int overrideWidth = 0;
static int overrideHeight = 0;
static SDL_Texture *penTexture = nullptr;

static std::vector<SDL_Vertex> penVerts;

// Flush pending pen geometry once this many vertices have accumulated.
static constexpr size_t PEN_FLUSH_THRESHOLD = 6144;

// Stamps queued since SDL's command queue was last flushed.
static constexpr unsigned PEN_STAMP_FLUSH_INTERVAL = 256;
static unsigned stampsSinceFlush = 0;

// Pen texture metrics, cached when the texture is (re)created instead of queried on every pen call.
static float penTexW = 640.0f;
static float penTexH = 480.0f;
static double penScaleY = 1.0;

static void refreshPenMetrics() {
    if (!penTexture) return;
    SDL_GetTextureSize(penTexture, &penTexW, &penTexH);
    penScaleY = penTexH / static_cast<double>(Scratch::projectHeight);
}

// Render target handling.
// Switching SDL render targets flushes SDL's command queue and makes the GPU wait for idle, so it is
// expensive. The pen texture therefore stays the active target across consecutive pen operations
// (lines, dots, stamps) and the main target is restored lazily, before anything else draws.
static bool penTargetActive = false;

static void ensurePenTarget(SDL_Renderer *renderer) {
    if (penTargetActive || !penTexture) return;
    SDL_SetRenderTarget(renderer, penTexture);
    penTargetActive = true;
    stampsSinceFlush = 0; // switching targets flushes the queue
}

static void ensureMainTarget(SDL_Renderer *renderer) {
    if (!penTargetActive) return;
    SDL_SetRenderTarget(renderer, mainRenderTarget);
    penTargetActive = false;
    stampsSinceFlush = 0;
}

// Draws any queued pen geometry into the pen texture. Leaves the pen texture as the active target.
static void flushPenVerts(SDL_Renderer *renderer) {
    if (penVerts.empty() || !penTexture) return;
    ensurePenTarget(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    constexpr size_t BATCH_SIZE = 2048 * 3; // 6144 vertices
    for (size_t i = 0; i < penVerts.size(); i += BATCH_SIZE) {
        size_t count = std::min(BATCH_SIZE, penVerts.size() - i);
        SDL_RenderGeometry(renderer, nullptr, &penVerts[i], static_cast<int>(count), nullptr, 0);
        SDL_FlushRenderer(renderer);
    }
    penVerts.clear();
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    stampsSinceFlush = 0;
}

// Pen circle geometry. cos/sin of the circle points are computed once per segment count.
struct CirclePoint {
    float c, s;
};

static const CirclePoint *getCircleTable(unsigned int segments) {
    static std::vector<std::vector<CirclePoint>> tables;
    if (segments >= tables.size()) tables.resize(segments + 1);
    std::vector<CirclePoint> &table = tables[segments];
    if (table.empty()) {
        table.resize(segments + 1);
        const double step = 2.0 * M_PI / segments;
        for (unsigned int i = 0; i <= segments; ++i) {
            table[i] = {static_cast<float>(std::cos(i * step)), static_cast<float>(std::sin(i * step))};
        }
    }
    return table.data();
}

static unsigned int circleSegmentsFor(double base, double penSize) {
    const double wanted = std::max(8.0, base * (penSize / 150.0));
    return std::min(static_cast<unsigned int>(wanted), 128u);
}

static void emitCircle(float cx, float cy, float radius, const SDL_FColor &color, const CirclePoint *table, unsigned int segments) {
    for (unsigned int i = 0; i < segments; ++i) {
        penVerts.push_back({{cx, cy}, color, {0.0f, 0.0f}});
        penVerts.push_back({{cx + table[i].c * radius, cy + table[i].s * radius}, color, {0.0f, 0.0f}});
        penVerts.push_back({{cx + table[i + 1].c * radius, cy + table[i + 1].s * radius}, color, {0.0f, 0.0f}});
    }
}

// The end cap of the last accurate pen move. When the next move continues from the same point with the
// same opaque color and size, its start cap would only repaint the same pixels, so it is skipped.
static bool lastCapValid = false;
static float lastCapX = 0.0f, lastCapY = 0.0f, lastCapRadius = 0.0f;
static SDL_FColor lastCapColor = {0, 0, 0, 0};

static inline bool sameColor(const SDL_FColor &a, const SDL_FColor &b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static void drawBlackBars(SDL_Renderer *renderer, int screenWidth, int screenHeight) {
    float screenAspect = static_cast<float>(screenWidth) / screenHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;

    if (screenAspect > projectAspect) {
        float scale = static_cast<float>(screenHeight) / Scratch::projectHeight;
        float scaledProjectWidth = Scratch::projectWidth * scale;
        float barWidth = (screenWidth - scaledProjectWidth) / 2.0f;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_FRect leftBar = {0, 0, std::ceil(barWidth), static_cast<float>(screenHeight)};
        SDL_FRect rightBar = {std::floor(screenWidth - barWidth), 0, std::ceil(barWidth), static_cast<float>(screenHeight)};

        SDL_RenderFillRect(renderer, &leftBar);
        SDL_RenderFillRect(renderer, &rightBar);
    } else if (screenAspect < projectAspect) {
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

Renderer_Xbox::Renderer_Xbox() {
    int windowWidth = 640;
    int windowHeight = 480;

    window = createWindow(windowWidth, windowHeight, "Scratch Everywhere!", WindowingBackends::SDL3);
    if (window->error.has_value()) {
        error = "Failed to init window: " + window->error.value();
        delete window;
        window = nullptr;
        return;
    }
    Sdlrenderer = static_cast<SDL_Renderer *>(static_cast<WindowSDL3 *>(window)->getRenderer());
    if (Sdlrenderer == nullptr) {
        error = "Could not create renderer: " + std::string(SDL_GetError());
        return;
    }
}

Renderer_Xbox::~Renderer_Xbox() {
    if (speechManager) {
        delete speechManager;
        speechManager = nullptr;
    }
    if (penTexture != nullptr) {
        ensureMainTarget(Sdlrenderer);
        SDL_DestroyTexture(penTexture);
        penTexture = nullptr;
    }

    TextObject::cleanupText();
    SDL_DestroyRenderer(Sdlrenderer);

    if (window) {
        window->cleanup();
        delete window;
        window = nullptr;
    }

    SDL_Quit();
}

void Renderer_Xbox::setRenderTarget(void *renderTarget, int width, int height) {
    mainRenderTarget = static_cast<SDL_Texture *>(renderTarget);
    overrideWidth = width;
    overrideHeight = height;
    hasOverrideTarget = true;
    penTargetActive = false; // the line below makes the main target current
    SDL_SetRenderTarget(Sdlrenderer, mainRenderTarget);
}

void Renderer_Xbox::clearRenderTarget() {
    mainRenderTarget = nullptr;
    hasOverrideTarget = false;
    penTargetActive = false;
    SDL_SetRenderTarget(Sdlrenderer, mainRenderTarget);
}

bool Renderer_Xbox::createSpeechManager() {
    if (speechManager == nullptr) speechManager = new SpeechManagerXbox();
    return speechManager != nullptr;
}

void Renderer_Xbox::destroySpeechManager() {
    delete speechManager;
    speechManager = nullptr;
}

SpeechManager *Renderer_Xbox::getSpeechManager() {
    if (speechManager == nullptr) speechManager = new SpeechManagerXbox();
    return speechManager;
}

int Renderer_Xbox::getWidth() {
    if (hasOverrideTarget) return overrideWidth;
    if (window) return window->getWidth();
    return 480;
}

int Renderer_Xbox::getHeight() {
    if (hasOverrideTarget) return overrideHeight;
    if (window) return window->getHeight();
    return 360;
}

float Renderer_Xbox::getPixelDensity() {
    if (window) return window->getPixelDensity();
    return 1.0f;
}

bool Renderer_Xbox::initPen() {
    if (penTexture != nullptr) return true;

    constexpr auto penFormat = SDL_PIXELFORMAT_ARGB8888;

    if (Scratch::hqpen) {
        if (Scratch::projectWidth / static_cast<double>(getWidth()) < Scratch::projectHeight / static_cast<double>(getHeight()))
            penTexture = SDL_CreateTexture(Sdlrenderer, penFormat, SDL_TEXTUREACCESS_TARGET, Scratch::projectWidth * (getHeight() / static_cast<double>(Scratch::projectHeight)), getHeight());
        else
            penTexture = SDL_CreateTexture(Sdlrenderer, penFormat, SDL_TEXTUREACCESS_TARGET, getWidth(), Scratch::projectHeight * (getWidth() / static_cast<double>(Scratch::projectWidth)));
    } else penTexture = SDL_CreateTexture(Sdlrenderer, penFormat, SDL_TEXTUREACCESS_TARGET, Scratch::projectWidth, Scratch::projectHeight);

    if (!penTexture) {
        Scratch::freeUnusedCostumeImages();
        if (Scratch::hqpen) {
            if (Scratch::projectWidth / static_cast<double>(getWidth()) < Scratch::projectHeight / static_cast<double>(getHeight()))
                penTexture = SDL_CreateTexture(Sdlrenderer, penFormat, SDL_TEXTUREACCESS_TARGET, Scratch::projectWidth * (getHeight() / static_cast<double>(Scratch::projectHeight)), getHeight());
            else
                penTexture = SDL_CreateTexture(Sdlrenderer, penFormat, SDL_TEXTUREACCESS_TARGET, getWidth(), Scratch::projectHeight * (getWidth() / static_cast<double>(Scratch::projectWidth)));
        } else penTexture = SDL_CreateTexture(Sdlrenderer, penFormat, SDL_TEXTUREACCESS_TARGET, Scratch::projectWidth, Scratch::projectHeight);
    }
    if (!penTexture) {
        Log::logError(std::string("Failed to create pen texture: ") + SDL_GetError());
        return false;
    }

    // Clear the texture
    SDL_SetTextureBlendMode(penTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(penTexture, SDL_SCALEMODE_NEAREST);
    SDL_SetRenderTarget(Sdlrenderer, penTexture);
    SDL_SetRenderDrawColor(Sdlrenderer, 0, 0, 0, 0);
    SDL_RenderClear(Sdlrenderer);
    SDL_SetRenderTarget(Sdlrenderer, mainRenderTarget);
    penTargetActive = false;

    refreshPenMetrics();
    lastCapValid = false;
    penVerts.clear();
    penVerts.reserve(PEN_FLUSH_THRESHOLD + 1024);

    return true;
}

void Renderer_Xbox::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
    if (!penTexture) return;
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    const float alpha = (100.0f - sprite->penData.color.transparency) / 100.0f;

    const double scale = penScaleY;

    const float sx1 = static_cast<float>(x1 * scale + penTexW / 2.0);
    const float sy1 = static_cast<float>(-y1 * scale + penTexH / 2.0);
    const float sx2 = static_cast<float>(x2 * scale + penTexW / 2.0);
    const float sy2 = static_cast<float>(-y2 * scale + penTexH / 2.0);

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

    const unsigned int circleSegments = circleSegmentsFor(8.0, sprite->penData.size);
    const CirclePoint *table = getCircleTable(circleSegments);
    const float radius = static_cast<float>(drawWidth);

    // Skip the start cap when this move continues the previous one (opaque pens only).
    const bool continuesChain = alpha >= 1.0f && lastCapValid && lastCapX == sx1 && lastCapY == sy1 &&
                                lastCapRadius == radius && sameColor(lastCapColor, sdlColor);
    if (!continuesChain) emitCircle(sx1, sy1, radius, sdlColor, table, circleSegments);
    emitCircle(sx2, sy2, radius, sdlColor, table, circleSegments);

    lastCapValid = true;
    lastCapX = sx2;
    lastCapY = sy2;
    lastCapRadius = radius;
    lastCapColor = sdlColor;

    if (penVerts.size() >= PEN_FLUSH_THRESHOLD) {
        flushPenVerts(Sdlrenderer);
    }
}

void Renderer_Xbox::penDotAccurate(Sprite *sprite) {
    if (!penTexture) return;
    lastCapValid = false;
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    const float alpha = (100.0f - sprite->penData.color.transparency) / 100.0f;

    const double scale = penScaleY;

    const float sx = static_cast<float>(sprite->xPosition * scale + penTexW / 2.0);
    const float sy = static_cast<float>(-sprite->yPosition * scale + penTexH / 2.0);

    const float radius = static_cast<float>((sprite->penData.size / 2.0f) * scale);

    const SDL_FColor sdlColor = {
        rgbColor.r / 255.0f,
        rgbColor.g / 255.0f,
        rgbColor.b / 255.0f,
        alpha};

    const unsigned int circleSegments = circleSegmentsFor(16.0, sprite->penData.size);
    emitCircle(sx, sy, radius, sdlColor, getCircleTable(circleSegments), circleSegments);

    if (penVerts.size() >= PEN_FLUSH_THRESHOLD) {
        flushPenVerts(Sdlrenderer);
    }
}

void Renderer_Xbox::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
    if (!penTexture) return;
    lastCapValid = false;
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    float alpha = (100.0 - sprite->penData.color.transparency) / 100.0;

    const double scale = penScaleY;

    const float sx1 = static_cast<float>(x1 * scale + penTexW / 2.0);
    const float sy1 = static_cast<float>(-y1 * scale + penTexH / 2.0);
    const float sx2 = static_cast<float>(x2 * scale + penTexW / 2.0);
    const float sy2 = static_cast<float>(-y2 * scale + penTexH / 2.0);

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

    if (penVerts.size() >= PEN_FLUSH_THRESHOLD) {
        flushPenVerts(Sdlrenderer);
    }
}

void Renderer_Xbox::penDotFast(Sprite *sprite) {
    if (!penTexture) return;
    lastCapValid = false;
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    float alpha = (100.0 - sprite->penData.color.transparency) / 100.0;

    const double scale = penScaleY;

    const float sx = static_cast<float>(sprite->xPosition * scale + penTexW / 2.0);
    const float sy = static_cast<float>(-sprite->yPosition * scale + penTexH / 2.0);

    const float halfSize = static_cast<float>((sprite->penData.size / 2.0f) * scale);

    const SDL_FColor sdlColor = {
        static_cast<float>(rgbColor.r / 255.0f),
        static_cast<float>(rgbColor.g / 255.0f),
        static_cast<float>(rgbColor.b / 255.0f),
        alpha};

    const SDL_Vertex v0 = {{sx - halfSize, sy - halfSize}, sdlColor, {0.0f, 0.0f}};
    const SDL_Vertex v1 = {{sx - halfSize, sy + halfSize}, sdlColor, {0.0f, 0.0f}};
    const SDL_Vertex v2 = {{sx + halfSize, sy - halfSize}, sdlColor, {0.0f, 0.0f}};
    const SDL_Vertex v3 = {{sx + halfSize, sy + halfSize}, sdlColor, {0.0f, 0.0f}};

    penVerts.push_back(v0);
    penVerts.push_back(v1);
    penVerts.push_back(v2);

    penVerts.push_back(v1);
    penVerts.push_back(v3);
    penVerts.push_back(v2);

    if (penVerts.size() >= PEN_FLUSH_THRESHOLD) {
        flushPenVerts(Sdlrenderer);
    }
}

void Renderer_Xbox::penStamp(Sprite *sprite) {
    if (!penTexture) return;
    lastCapValid = false;

    // Queued lines must be drawn first so the stamp lands on top of them. This leaves the pen texture
    // as the active target, and it stays active for the next pen operation.
    flushPenVerts(Sdlrenderer);

    auto imgFind = Scratch::costumeImages.find(sprite->costumes[sprite->currentCostume].fullName);
    if (imgFind == Scratch::costumeImages.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return;
    }

    const Costume &costume = sprite->costumes[sprite->currentCostume];

    ensurePenTarget(Sdlrenderer);

    Image_Xbox *image = reinterpret_cast<Image_Xbox *>(imgFind->second.get());

    const bool isSVG = costume.isSVG;
    Render::calculateRenderPosition(sprite, isSVG);

    // Pen mapping stuff
    const auto &cords = Scratch::screenToScratchCoords(sprite->renderInfo.renderX, sprite->renderInfo.renderY, getWidth(), getHeight());
    int penX = cords.first + Scratch::projectWidth / 2;
    int penY = -cords.second + Scratch::projectHeight / 2;

    float penScale;
    if (Scratch::hqpen) {
        const double scale = penScaleY;

        penX *= scale;
        penY *= scale;
        penScale = sprite->renderInfo.renderScaleY;
    } else {
        penScale = (sprite->size / 100.0f) / costume.bitmapResolution;
        if (Scratch::bitmapHalfQuality && !costume.isSVG && costume.bitmapResolution == 2) {
            penScale *= 2.0f;
        }
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

    if (++stampsSinceFlush >= PEN_STAMP_FLUSH_INTERVAL) {
        SDL_FlushRenderer(Sdlrenderer);
        stampsSinceFlush = 0;
    }
}

void Renderer_Xbox::penClear() {
    if (!penTexture) return;
    penVerts.clear(); // pending lines would be erased by the clear anyway
    lastCapValid = false;

    ensurePenTarget(Sdlrenderer);
    SDL_SetRenderDrawColor(Sdlrenderer, 0, 0, 0, 0);
    SDL_RenderClear(Sdlrenderer);

    // Force the command queue to flush the clear operation.
    // If a Stamp immediately follows a Clear, the Stamp's standard geometry flush
    // gets skipped (because penVerts is empty). This leaves the nxdk GPU transitioning
    // directly from a Clear to a Texture draw in the same batch, corrupting the cache.
    // Flushing here resolves it without an expensive render target switch.
    SDL_FlushRenderer(Sdlrenderer);
}

void Renderer_Xbox::cleanupPen() {
    if (penTexture != nullptr) {
        ensureMainTarget(Sdlrenderer);
        SDL_DestroyTexture(penTexture);
        penTexture = nullptr;
    }
    penTargetActive = false;
    lastCapValid = false;
    penVerts.clear();
    penVerts.shrink_to_fit();
}

void Renderer_Xbox::beginFrame(int screen, int colorR, int colorG, int colorB) {
    if (!hasFrameBegan) {
        ensureMainTarget(Sdlrenderer);
        SDL_SetRenderDrawColor(Sdlrenderer, colorR, colorG, colorB, 255);
        SDL_RenderClear(Sdlrenderer);
        hasFrameBegan = true;
    }
}

void Renderer_Xbox::endFrame(bool shouldFlush) {
    ensureMainTarget(Sdlrenderer);
    SDL_RenderPresent(Sdlrenderer);
    SDL_Delay(16);
    hasFrameBegan = false;
}

void Renderer_Xbox::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    ensureMainTarget(Sdlrenderer);
    SDL_SetRenderDrawColor(Sdlrenderer, colorR, colorG, colorB, colorA);
    SDL_FRect rect = {x - (w / 2.0f), y - (h / 2.0f), static_cast<float>(w), static_cast<float>(h)};
    SDL_RenderFillRect(Sdlrenderer, &rect);
}

void Renderer_Xbox::drawRoundedBox(int w, int h, int x, int y, int radius, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    radius = std::clamp(radius, 0, std::min(w, h) / 2);
    if (radius <= 0) {
        drawBox(w, h, x, y, colorR, colorG, colorB, colorA);
        return;
    }

    ensureMainTarget(Sdlrenderer);

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

void Renderer_Xbox::renderSprites() {
    // Everything below draws to the main target; pen operations may have left the pen texture active.
    ensureMainTarget(Sdlrenderer);

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

void Renderer_Xbox::renderPenLayer() {
    if (!penTexture) return;
    flushPenVerts(Sdlrenderer);
    ensureMainTarget(Sdlrenderer);

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

bool Renderer_Xbox::appShouldRun() {
    if (OS::toExit) return false;
    if (window) {
        // Make sure nothing outside the pen code draws into the pen texture.
        ensureMainTarget(Sdlrenderer);

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