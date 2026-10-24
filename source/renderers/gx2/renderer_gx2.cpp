#include "renderer_gx2.hpp"
#include "gx2_gfx.hpp"
#include "image_gx2.hpp"
#include "speech_manager_gx2.hpp"
#include <log.hpp>
#include <runtime.hpp>
#include <window.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static GX2Gfx::RenderTarget sceneTarget;
static GX2Gfx::RenderTarget penTarget;
static int penWidth = 0;
static int penHeight = 0;

static bool penPassActive = false;
static bool penDirty = false;

static std::vector<float> penBatchVerts;
static constexpr size_t kPenBatchFlushThreshold = 60000 * 6; // ~60k vertices

static GX2ColorBuffer *overrideColorBuffer = nullptr;
static bool hasOverrideTarget = false;
static int overrideWidth = 0;
static int overrideHeight = 0;

static int effectiveWidth(WindowSE *window) {
    if (hasOverrideTarget) return overrideWidth;
    return window ? window->getWidth() : 854;
}

static int effectiveHeight(WindowSE *window) {
    if (hasOverrideTarget) return overrideHeight;
    return window ? window->getHeight() : 480;
}

static GX2ColorBuffer *mainColorBuffer() {
    return hasOverrideTarget ? overrideColorBuffer : &sceneTarget.colorBuffer;
}

static void bindMainTarget(WindowSE *window) {
    GX2Gfx::setRenderTarget(mainColorBuffer(), effectiveWidth(window), effectiveHeight(window));
}

static constexpr uint32_t kSolidStride = 6 * sizeof(float);

static void setSolidState(const float proj[16]) {
    const GX2Gfx::SolidProgram &program = GX2Gfx::solidProgram();
    GX2Gfx::bindProgram(program);
    GX2Gfx::setBlend(GX2Gfx::Blend::Alpha);
    GX2Gfx::setVertexUniform(program.uProjection, proj, 16);
}

static void drawSolid(const float *verts, size_t floatCount, GX2PrimitiveMode mode, const float proj[16]) {
    const size_t bytes = floatCount * sizeof(float);
    float *stream = static_cast<float *>(GX2Gfx::streamAllocate(bytes));
    if (!stream) return;
    std::copy(verts, verts + floatCount, stream);

    setSolidState(proj);
    GX2Gfx::drawStream(stream, bytes, kSolidStride, mode, static_cast<uint32_t>(floatCount / 6));
}

static void drawSolidRect(float x, float y, float w, float h,
                          float r, float g, float b, float a,
                          const float proj[16]) {
    const float verts[] = {
        x, y, r, g, b, a,
        x + w, y, r, g, b, a,
        x + w, y + h, r, g, b, a,
        x, y + h, r, g, b, a};
    drawSolid(verts, 24, GX2_PRIMITIVE_MODE_QUADS, proj);
}

static void drawSolidFan(const std::vector<float> &verts, const float proj[16]) {
    drawSolid(verts.data(), verts.size(), GX2_PRIMITIVE_MODE_TRIANGLE_FAN, proj);
}

static int segmentsForRadius(float radius, int maxSegments) {
    constexpr int kMinSegments = 6;
    int segments = static_cast<int>(std::ceil(radius * 1.5f));
    return std::clamp(segments, kMinSegments, maxSegments);
}

static void appendSolidCircleTris(std::vector<float> &out, float cx, float cy, float radius,
                                  float r, float g, float b, float a, int segments = -1) {
    if (segments <= 0) segments = segmentsForRadius(radius, 24);
    const float twoPi = 2.0f * static_cast<float>(M_PI);
    float prevX = cx + radius, prevY = cy;
    out.reserve(out.size() + segments * 3 * 6);
    for (int i = 1; i <= segments; ++i) {
        float angle = i * twoPi / segments;
        float curX = cx + std::cos(angle) * radius;
        float curY = cy + std::sin(angle) * radius;
        out.insert(out.end(), {cx, cy, r, g, b, a,
                               prevX, prevY, r, g, b, a,
                               curX, curY, r, g, b, a});
        prevX = curX;
        prevY = curY;
    }
}

static void appendSolidCapsuleTris(std::vector<float> &out, float x1, float y1, float x2, float y2,
                                   float radius, float r, float g, float b, float a, int capSegments = -1) {
    if (capSegments <= 0) capSegments = segmentsForRadius(radius, 12);

    float dx = x2 - x1, dy = y2 - y1;
    float length = std::sqrt(dx * dx + dy * dy);

    if (length <= 0.001f) {
        appendSolidCircleTris(out, x1, y1, radius, r, g, b, a, capSegments * 2);
        return;
    }

    float phi = std::atan2(dy, dx);
    std::vector<float> ring;
    ring.reserve(((capSegments + 1) * 2 + 1) * 2);

    float startAngle2 = phi - static_cast<float>(M_PI_2);
    for (int i = 0; i <= capSegments; ++i) {
        float angle = startAngle2 + (i * static_cast<float>(M_PI) / capSegments);
        ring.push_back(x2 + std::cos(angle) * radius);
        ring.push_back(y2 + std::sin(angle) * radius);
    }

    float startAngle1 = phi + static_cast<float>(M_PI_2);
    for (int i = 0; i <= capSegments; ++i) {
        float angle = startAngle1 + (i * static_cast<float>(M_PI) / capSegments);
        ring.push_back(x1 + std::cos(angle) * radius);
        ring.push_back(y1 + std::sin(angle) * radius);
    }

    const float centerX = (x1 + x2) * 0.5f, centerY = (y1 + y2) * 0.5f;
    const size_t ringCount = ring.size() / 2;
    out.reserve(out.size() + ringCount * 3 * 6);
    for (size_t i = 0; i < ringCount; ++i) {
        size_t next = (i + 1) % ringCount;
        out.insert(out.end(), {centerX, centerY, r, g, b, a,
                               ring[i * 2], ring[i * 2 + 1], r, g, b, a,
                               ring[next * 2], ring[next * 2 + 1], r, g, b, a});
    }
}

static void flushPenBatch() {
    if (penBatchVerts.empty()) return;

    float proj[16];
    GX2Gfx::buildOrtho(proj, 0.0f, static_cast<float>(penWidth), static_cast<float>(penHeight), 0.0f);

    constexpr size_t kChunkFloats = 6 * 3 * 20000; // 20k triangles
    for (size_t offset = 0; offset < penBatchVerts.size(); offset += kChunkFloats) {
        const size_t count = std::min(kChunkFloats, penBatchVerts.size() - offset);
        drawSolid(penBatchVerts.data() + offset, count, GX2_PRIMITIVE_MODE_TRIANGLES, proj);
    }

    penBatchVerts.clear();
}

static void discardPenBatch() {
    penBatchVerts.clear();
}

static bool createPenTarget(WindowSE *window) {
    if (Scratch::hqpen) {
        const int renderWidth = effectiveWidth(window);
        const int renderHeight = effectiveHeight(window);
        if (Scratch::projectWidth / static_cast<double>(renderWidth) <
            Scratch::projectHeight / static_cast<double>(renderHeight)) {
            penWidth = Scratch::projectWidth * (renderHeight / static_cast<double>(Scratch::projectHeight));
            penHeight = renderHeight;
        } else {
            penWidth = renderWidth;
            penHeight = Scratch::projectHeight * (renderWidth / static_cast<double>(Scratch::projectWidth));
        }
    } else {
        penWidth = Scratch::projectWidth;
        penHeight = Scratch::projectHeight;
    }

    if (!GX2Gfx::createRenderTarget(penTarget, penWidth, penHeight)) {
        Log::logError("[GX2] Failed to create the pen render target");
        penWidth = penHeight = 0;
        return false;
    }

    GX2Gfx::setRenderTarget(&penTarget.colorBuffer, penWidth, penHeight);
    GX2Gfx::clear(0.0f, 0.0f, 0.0f, 0.0f);
    bindMainTarget(window);
    penPassActive = false;
    penDirty = true;
    return true;
}

static void destroyPenTarget() {
    discardPenBatch();
    if (penPassActive) penPassActive = false;
    GX2Gfx::destroyRenderTarget(penTarget);
    penWidth = penHeight = 0;
    penDirty = false;
}

static void flushPenTarget(WindowSE *window) {
    if (!penPassActive) return;
    flushPenBatch();
    bindMainTarget(window);
    penPassActive = false;
}

static void makePenReadable(WindowSE *window) {
    if (!penDirty) return;
    flushPenTarget(window);
    GX2Gfx::makeReadable(penTarget);
    penDirty = false;
}

static void penBegin(WindowSE *window) {
    penDirty = true;
    if (penPassActive) return;
    (void)window;
    GX2Gfx::setRenderTarget(&penTarget.colorBuffer, penWidth, penHeight);
    penPassActive = true;
}

static bool rectOverlapsPenCanvas(float minX, float minY, float maxX, float maxY) {
    return maxX >= 0.0f && maxY >= 0.0f && minX <= static_cast<float>(penWidth) && minY <= static_cast<float>(penHeight);
}

static void drawBlackBars(int screenWidth, int screenHeight, const float proj[16]) {
    float screenAspect = static_cast<float>(screenWidth) / screenHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;

    if (screenAspect > projectAspect) {
        float scale = static_cast<float>(screenHeight) / Scratch::projectHeight;
        float scaledProjectWidth = Scratch::projectWidth * scale;
        float barWidth = (screenWidth - scaledProjectWidth) / 2.0f;

        drawSolidRect(0, 0, barWidth, static_cast<float>(screenHeight), 0, 0, 0, 1, proj);
        drawSolidRect(screenWidth - barWidth, 0, barWidth, static_cast<float>(screenHeight), 0, 0, 0, 1, proj);
    } else if (screenAspect < projectAspect) {
        float scale = static_cast<float>(screenWidth) / Scratch::projectWidth;
        float scaledProjectHeight = Scratch::projectHeight * scale;
        float barHeight = (screenHeight - scaledProjectHeight) / 2.0f;

        drawSolidRect(0, 0, static_cast<float>(screenWidth), barHeight, 0, 0, 0, 1, proj);
        drawSolidRect(0, screenHeight - barHeight, static_cast<float>(screenWidth), barHeight, 0, 0, 0, 1, proj);
    }
}

Renderer_GX2::Renderer_GX2() {
    window = createWindow(854, 480, "Scratch Everywhere!", WindowingBackends::WiiU);

    if (window->error.has_value()) {
        error = "Failed to init window: " + window->error.value();
        delete window;
        window = nullptr;
        return;
    }

    if (const char *gfxError = GX2Gfx::init(window->getWidth(), window->getHeight())) {
        Log::logError(std::string("[GX2] ") + gfxError);
        window->cleanup();
        delete window;
        window = nullptr;
        error = gfxError;
        return;
    }

    if (!GX2Gfx::createRenderTarget(sceneTarget, window->getWidth(), window->getHeight())) {
        GX2Gfx::shutdown();
        window->cleanup();
        delete window;
        window = nullptr;
        error = "Failed to create the scene render target.";
        return;
    }

    Log::log("[GX2] Scene resolution: " + std::to_string(window->getWidth()) + "x" + std::to_string(window->getHeight()));
    bindMainTarget(window);
}

Renderer_GX2::~Renderer_GX2() {
    if (GX2Gfx::isInitialised()) GX2Gfx::waitIdle();

    destroyPenTarget();

    TextObject::cleanupText();

    if (speechManager) {
        delete speechManager;
        speechManager = nullptr;
    }

    GX2Gfx::destroyRenderTarget(sceneTarget);
    GX2Gfx::shutdown();

    if (window) {
        window->cleanup();
        delete window;
        window = nullptr;
    }
}

void Renderer_GX2::setRenderTarget(void *renderTarget, int width, int height) {
    flushPenTarget(window);

    const bool sizeChanged = hasOverrideTarget && (overrideWidth != width || overrideHeight != height);

    overrideColorBuffer = static_cast<GX2ColorBuffer *>(renderTarget);
    overrideWidth = width;
    overrideHeight = height;
    hasOverrideTarget = true;
    bindMainTarget(window);

    if (sizeChanged && Scratch::hqpen) {
        destroyPenTarget();
        createPenTarget(window);
    }
}

void Renderer_GX2::clearRenderTarget() {
    flushPenTarget(window);
    hasOverrideTarget = false;
    overrideColorBuffer = nullptr;
    overrideWidth = 0;
    overrideHeight = 0;
    bindMainTarget(window);
}

bool Renderer_GX2::createSpeechManager() {
    if (speechManager == nullptr) speechManager = new SpeechManagerGX2();
    return speechManager != nullptr;
}

void Renderer_GX2::destroySpeechManager() {
    delete speechManager;
    speechManager = nullptr;
}

SpeechManager *Renderer_GX2::getSpeechManager() { return speechManager; }

int Renderer_GX2::getWidth() { return effectiveWidth(window); }
int Renderer_GX2::getHeight() { return effectiveHeight(window); }

float Renderer_GX2::getPixelDensity() {
    return window ? window->getPixelDensity() : 1.0f;
}

bool Renderer_GX2::initPen() {
    if (penTarget.valid) return true;
    return createPenTarget(window);
}

void Renderer_GX2::penClear() {
    if (!penTarget.valid) return;
    discardPenBatch();

    GX2Gfx::setRenderTarget(&penTarget.colorBuffer, penWidth, penHeight);
    GX2Gfx::clear(0.0f, 0.0f, 0.0f, 0.0f);
    bindMainTarget(window);

    penPassActive = false;
    penDirty = true;
}

void Renderer_GX2::cleanupPen() {
    discardPenBatch();
    destroyPenTarget();
}

void Renderer_GX2::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
    penMoveAccurate(x1, y1, x2, y2, sprite);
}

void Renderer_GX2::penDotFast(Sprite *sprite) {
    penDotAccurate(sprite);
}

void Renderer_GX2::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
    if (!penTarget.valid) return;

    const double scale = penHeight / static_cast<double>(Scratch::projectHeight);
    float px1 = static_cast<float>(x1 * scale + penWidth / 2.0);
    float py1 = static_cast<float>(-y1 * scale + penHeight / 2.0);
    float px2 = static_cast<float>(x2 * scale + penWidth / 2.0);
    float py2 = static_cast<float>(-y2 * scale + penHeight / 2.0);
    float radius = static_cast<float>((sprite->penData.size / 2.0) * scale);

    if (!rectOverlapsPenCanvas(std::min(px1, px2) - radius, std::min(py1, py2) - radius,
                               std::max(px1, px2) + radius, std::max(py1, py2) + radius))
        return;

    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    float alpha = (100.0f - static_cast<float>(sprite->penData.color.transparency)) / 100.0f;
    float r = rgbColor.r / 255.0f, g = rgbColor.g / 255.0f, b = rgbColor.b / 255.0f;

    penBegin(window);
    appendSolidCapsuleTris(penBatchVerts, px1, py1, px2, py2, radius, r, g, b, alpha);
    if (penBatchVerts.size() >= kPenBatchFlushThreshold) flushPenBatch();
}

void Renderer_GX2::penDotAccurate(Sprite *sprite) {
    if (!penTarget.valid) return;

    const double scale = penHeight / static_cast<double>(Scratch::projectHeight);
    float px = static_cast<float>(sprite->xPosition * scale + penWidth / 2.0);
    float py = static_cast<float>(-sprite->yPosition * scale + penHeight / 2.0);
    float radius = static_cast<float>((sprite->penData.size / 2.0) * scale);

    if (!rectOverlapsPenCanvas(px - radius, py - radius, px + radius, py + radius)) return;

    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    float alpha = (100.0f - static_cast<float>(sprite->penData.color.transparency)) / 100.0f;
    float r = rgbColor.r / 255.0f, g = rgbColor.g / 255.0f, b = rgbColor.b / 255.0f;

    penBegin(window);
    appendSolidCircleTris(penBatchVerts, px, py, radius, r, g, b, alpha);
    if (penBatchVerts.size() >= kPenBatchFlushThreshold) flushPenBatch();
}

void Renderer_GX2::penStamp(Sprite *sprite) {
    if (!penTarget.valid) return;

    const auto &imgFind = Scratch::costumeImages.find(sprite->costumes[sprite->currentCostume].fullName);
    if (imgFind == Scratch::costumeImages.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return;
    }

    Image_GX2 *image = reinterpret_cast<Image_GX2 *>(imgFind->second.get());

    const bool isSVG = sprite->costumes[sprite->currentCostume].isSVG;
    Render::calculateRenderPosition(sprite, isSVG);

    auto cords = Scratch::screenToScratchCoords(sprite->renderInfo.renderX, sprite->renderInfo.renderY, getWidth(), getHeight());
    float penX = static_cast<float>(cords.first + Scratch::projectWidth / 2.0);
    float penY = static_cast<float>(-cords.second + Scratch::projectHeight / 2.0);

    const double scale = penHeight / static_cast<double>(Scratch::projectHeight);
    if (Scratch::hqpen) {
        penX *= static_cast<float>(scale);
        penY *= static_cast<float>(scale);
    }

    const Costume &costume = sprite->costumes[sprite->currentCostume];
    float renderScale = Scratch::hqpen ? sprite->renderInfo.renderScaleY : (sprite->size / 100.0f) / costume.bitmapResolution;

    const float halfDiag = 0.5f * std::sqrt(static_cast<float>(image->getWidth() * image->getWidth() + image->getHeight() * image->getHeight())) * std::abs(renderScale);
    if (!rectOverlapsPenCanvas(penX - halfDiag, penY - halfDiag, penX + halfDiag, penY + halfDiag)) return;

    ImageRenderParams params;
    params.centered = true;
    params.x = penX;
    params.y = penY;
    params.scale = renderScale;
    params.rotation = sprite->renderInfo.renderRotation;
    params.flip = (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0);
    params.opacity = 1.0f - std::clamp(sprite->ghostEffect, 0.0f, 100.0f) * 0.01f;
    params.brightness = sprite->brightnessEffect;
    params.blend = false;

    penBegin(window);
    flushPenBatch();
    image->render(params);
}

void Renderer_GX2::beginFrame(int /*screen*/, int colorR, int colorG, int colorB) {
    if (!hasFrameBegan) {
        GX2Gfx::beginFrame();
        flushPenTarget(window);
        bindMainTarget(window);
        GX2Gfx::clear(colorR / 255.0f, colorG / 255.0f, colorB / 255.0f, 1.0f);
        hasFrameBegan = true;
    }
}

void Renderer_GX2::endFrame(bool /*shouldFlush*/) {
    if (!hasOverrideTarget) {
        flushPenTarget(window);
        GX2Gfx::present(sceneTarget);
    }
    hasFrameBegan = false;
}

void Renderer_GX2::drawBox(int w, int h, int x, int y,
                           uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    float proj[16];
    GX2Gfx::buildOrtho(proj, 0.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()), 0.0f);

    drawSolidRect(static_cast<float>(x - w / 2), static_cast<float>(y - h / 2), static_cast<float>(w), static_cast<float>(h),
                  colorR / 255.0f, colorG / 255.0f, colorB / 255.0f, colorA / 255.0f,
                  proj);
}

void Renderer_GX2::drawRoundedBox(int w, int h, int x, int y, int radius,
                                  uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    radius = std::clamp(radius, 0, std::min(w, h) / 2);
    if (radius <= 0) {
        drawBox(w, h, x, y, colorR, colorG, colorB, colorA);
        return;
    }

    const float left = x - w / 2.0f, right = x + w / 2.0f;
    const float top = y - h / 2.0f, bottom = y + h / 2.0f;
    const float r = static_cast<float>(radius);
    const float cr = colorR / 255.0f, cg = colorG / 255.0f, cb = colorB / 255.0f, ca = colorA / 255.0f;

    constexpr int cornerSegments = 4;
    std::vector<float> verts;
    verts.reserve((cornerSegments + 1) * 4 * 6);

    auto addArc = [&](float cx, float cy, float startAngle) {
        for (int i = 0; i <= cornerSegments; ++i) {
            const float a = startAngle + (static_cast<float>(M_PI) / 2.0f) * (i / static_cast<float>(cornerSegments));
            verts.insert(verts.end(), {cx + std::cos(a) * r, cy + std::sin(a) * r, cr, cg, cb, ca});
        }
    };

    addArc(right - r, top + r, -static_cast<float>(M_PI) / 2.0f);
    addArc(right - r, bottom - r, 0.0f);
    addArc(left + r, bottom - r, static_cast<float>(M_PI) / 2.0f);
    addArc(left + r, top + r, static_cast<float>(M_PI));

    float proj[16];
    GX2Gfx::buildOrtho(proj, 0.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()), 0.0f);
    drawSolidFan(verts, proj);
}

void Renderer_GX2::renderPenLayer() {
    if (!penTarget.valid) return;

    makePenReadable(window);

    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;
    float windowAspect = static_cast<float>(getWidth()) / getHeight();

    float drawW, drawH, drawX, drawY;
    if (windowAspect > projectAspect) {
        drawH = static_cast<float>(getHeight());
        drawW = drawH * projectAspect;
        drawX = (getWidth() - drawW) / 2.0f;
        drawY = 0;
    } else {
        drawW = static_cast<float>(getWidth());
        drawH = drawW / projectAspect;
        drawX = 0;
        drawY = (getHeight() - drawH) / 2.0f;
    }

    float proj[16];
    GX2Gfx::buildOrtho(proj, 0.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()), 0.0f);

    float model[16] = {};
    model[0] = drawW;
    model[5] = drawH;
    model[10] = 1.0f;
    model[12] = drawX;
    model[13] = drawY;
    model[15] = 1.0f;

    GX2Gfx::SpriteParams params;
    params.texWidth = static_cast<float>(penWidth);
    params.texHeight = static_cast<float>(penHeight);

    const GX2Gfx::SpriteProgram &program = GX2Gfx::spriteProgram();
    GX2Gfx::bindProgram(program);
    GX2Gfx::setBlend(GX2Gfx::Blend::Premultiplied);
    GX2Gfx::setSpriteUniforms(model, proj, params);
    GX2Gfx::bindTexture(penTarget.texture, program.samplerLocation, GX2Gfx::Filter::Nearest);
    GX2Gfx::drawUnitQuad();
}

void Renderer_GX2::renderSprites() {
    GX2Gfx::beginFrame();
    flushPenTarget(window);
    bindMainTarget(window);
    GX2Gfx::clear(1.0f, 1.0f, 1.0f, 1.0f);

    float proj[16];
    GX2Gfx::buildOrtho(proj, 0.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()), 0.0f);

    for (auto it = Scratch::sprites.rbegin(); it != Scratch::sprites.rend(); ++it) {
        Sprite *currentSprite = *it;

        auto imgFind = Scratch::costumeImages.find(currentSprite->costumes[currentSprite->currentCostume].fullName);
        if (imgFind != Scratch::costumeImages.end()) {
            Image_GX2 *image = reinterpret_cast<Image_GX2 *>(imgFind->second.get());

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
            params.opacity = 1.0f - std::clamp(currentSprite->ghostEffect, 0.0f, 100.0f) * 0.01f;
            params.brightness = currentSprite->brightnessEffect;
            params.colorEffect = currentSprite->colorEffect;
            params.fisheyeEffect = currentSprite->fisheyeEffect;
            params.whirlEffect = currentSprite->whirlEffect;
            params.pixelateEffect = currentSprite->pixelateEffect;
            params.mosaicEffect = currentSprite->mosaicEffect;

            image->render(params);
        }

        if (currentSprite->isStage) renderPenLayer();
    }

    if (speechManager) speechManager->render();

    drawBlackBars(getWidth(), getHeight(), proj);
    Render::renderMonitors();

    endFrame(true);
}

bool Renderer_GX2::appShouldRun() {
    if (OS::toExit) return false;
    if (window) {
        window->pollEvents();

        static int lastW = 0, lastH = 0;
        int currentW = effectiveWidth(window);
        int currentH = effectiveHeight(window);

        if (lastW != currentW || lastH != currentH) {
            lastW = currentW;
            lastH = currentH;

            if (Scratch::hqpen) {
                destroyPenTarget();
                createPenTarget(window);
            }
        }

        return !window->shouldClose();
    }
    return false;
}
