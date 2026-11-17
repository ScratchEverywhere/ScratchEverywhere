#include <cstddef>
#include "costume_shaders_xbox360.hpp"
#include "pen_shaders_xbox360.hpp"
#include "texture_shaders_xbox360.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <input.hpp>
#include <log.hpp>
#include <os.hpp>
#include <renderer_xbox360.hpp>
#include <runtime/color.hpp>
#include <runtime/runtime.hpp>
#include <stdexcept>
#include <xsdk/gpu.h>
#include <xsdk/text.h>

namespace {
constexpr size_t kMaxPenBatchVerts = 4096;
constexpr float kPi = 3.14159265358979323846f;
} // namespace

Renderer_Xbox360::Renderer_Xbox360() {
    window = createWindow(1280, 720, "Scratch Everywhere!", WindowingBackends::Xbox360);
    surface_ = static_cast<WindowXbox360 *>(window);
    if (!surface_ || surface_->error) {
        error = surface_ ? *surface_->error : "Xbox 360 window factory returned null";
        return;
    }
    const NTSTATUS status = xsdk_text_open();
    if (!NT_SUCCESS(status)) {
        error = "xsdk_text_open failed: " + std::to_string(static_cast<uint32_t>(status));
        return;
    }
    textOpen_ = true;
    Log::log("Xbox 360 renderer initialized");
}
Renderer_Xbox360::~Renderer_Xbox360() {
    if (hasFrameBegan) endFrame(false);
    destroySpeechManager();
    releasePen();
    if (textOpen_) xsdk_text_close();
    delete window;
    window = nullptr;
    Log::log("Xbox 360 renderer shutdown complete");
}
void Renderer_Xbox360::beginFrame(int, int r, int g, int b) {
    // Menus submit both logical screens before endFrame on single-display backends.
    if (hasFrameBegan) return;
    if (!surface_->beginFrame()) {
        if (surface_->error) error = *surface_->error;
        Log::logWarning("Renderer_Xbox360::beginFrame failed");
        return;
    }
    static bool loggedFirstFrame = false;
    if (!loggedFirstFrame) {
        loggedFirstFrame = true;
        Log::log("Renderer_Xbox360: first frame begun (" + std::to_string(getWidth()) + "x" +
                 std::to_string(getHeight()) + ")");
    }
    background_ = 0xFF000000u | (uint32_t(std::clamp(r, 0, 255)) << 16) |
                  (uint32_t(std::clamp(g, 0, 255)) << 8) | uint32_t(std::clamp(b, 0, 255));
    xsdk_gpu_set_target(getWidth(), getHeight());
    // Resolve clears after copying: discard this copy to initialize the current frame.
    xsdk_gpu_resolve(surface_->frameAddress(), getWidth(), getHeight(), background_);
    xsdk_gpu_set_target(getWidth(), getHeight());
    hasFrameBegan = true;
}
void Renderer_Xbox360::endFrame(bool) {
    if (!hasFrameBegan) return;
    if (Input::mousePointer.isMoving) {
        auto cursor = Input::getTouchPosition();
        drawBox(12, 12, cursor[0], cursor[1], 0, 0, 0, 255);
        drawBox(8, 8, cursor[0] + 2, cursor[1] + 2, 255, 255, 255, 255);
    }
    xsdk_gpu_resolve(surface_->frameAddress(), getWidth(), getHeight(), background_);
    surface_->swapBuffers();
    hasFrameBegan = false;
}
void Renderer_Xbox360::drawBox(int w, int h, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!hasFrameBegan) throw std::logic_error("Xbox 360 drawBox requires beginFrame");
    if (w <= 0 || h <= 0 || a == 0) return;
    const int left = std::max(0, x), top = std::max(0, y);
    const int right = int(std::min<int64_t>(getWidth(), int64_t(x) + w));
    const int bottom = int(std::min<int64_t>(getHeight(), int64_t(y) + h));
    if (left >= right || top >= bottom) return;
    const uint32_t argb = (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
    xsdk_gpu_set_scissor(left, top, right - left, bottom - top);
    // Fill with the SDK's full-block glyph, clipped to exact box bounds. This uses
    // its existing GPU text shaders and avoids an unrelated shader compiler dependency.
    const int columns = (right - left + 79) / 80;
    std::string blocks;
    for (int i = 0; i < columns; ++i)
        blocks += "\u2588";
    for (int row = top; row < bottom; row += 160)
        xsdk_text_draw(left, row, 8, argb, blocks.c_str());
    xsdk_gpu_set_scissor(0, 0, getWidth(), getHeight());
}
int Renderer_Xbox360::getWidth() { return targetWidth_ > 0 ? targetWidth_ : surface_->getWidth(); }
int Renderer_Xbox360::getHeight() { return targetHeight_ > 0 ? targetHeight_ : surface_->getHeight(); }
float Renderer_Xbox360::getPixelDensity() { return 1.0f; }
bool Renderer_Xbox360::appShouldRun() {
    surface_->pollEvents();
    return !OS::toExit && !surface_->shouldClose();
}
[[noreturn]] void Renderer_Xbox360::unsupported(const char *feature) {
    const std::string message = std::string("Xbox 360 renderer does not support ") + feature;
    Log::logError(message);
    throw std::logic_error(message);
}
void Renderer_Xbox360::releasePen() {
    if (!penInitialized_ && !penQuadVerts_ && !penDrawVerts_) return;
    if (!NT_SUCCESS(xsdk_gpu_wait(xsdk_gpu_fence(), 5000))) {
        Log::logError("Pen GPU wait failed; retaining pen resources until title termination");
        penInitialized_ = false;
        return;
    }
    xsdk_gpu_free(penQuadVerts_);
    penQuadVerts_ = nullptr;
    xsdk_gpu_free(penDrawVerts_);
    penDrawVerts_ = nullptr;
    xsdk_shader_free(&penFrag_);
    xsdk_shader_free(&penVert_);
    xsdk_shader_free(&penTexFrag_);
    xsdk_shader_free(&penTexVert_);
    xsdk_texture_free(&penTexture_);
    penVerts_.clear();
    penReadback_.clear();
    penReadbackDirty_ = true;
    penReadbackRowStart_ = 0;
    penReadbackRowEnd_ = -1;
    penInitialized_ = false;
    penQuadUsed_ = false;
    penDrawUsed_ = false;
}

bool Renderer_Xbox360::initPen() {
    if (penInitialized_) return true;
    int penW = Scratch::projectWidth > 0 ? Scratch::projectWidth : 480;
    int penH = Scratch::projectHeight > 0 ? Scratch::projectHeight : 360;
    if (Scratch::hqpen) {
        penW = surface_->getWidth();
        penH = surface_->getHeight();
    }
    penW = std::clamp(penW, 1, int(XSDK_TEXTURE_MAX_SIZE));
    penH = std::clamp(penH, 1, int(XSDK_TEXTURE_MAX_SIZE));

    if (!NT_SUCCESS(xsdk_texture_create(&penTexture_, XSDK_FORMAT_R8G8B8A8, penW, penH, 1, false))) {
        Log::logError("Xbox 360 pen texture creation failed");
        return false;
    }
    if (!NT_SUCCESS(xsdk_shader_load(&penVert_, pen_vert.container, pen_vert.container_bytes)) ||
        !NT_SUCCESS(xsdk_shader_load(&penFrag_, pen_frag.container, pen_frag.container_bytes)) ||
        !NT_SUCCESS(xsdk_shader_load(&penTexVert_, texture_vertex_shader, sizeof texture_vertex_shader)) ||
        !NT_SUCCESS(xsdk_shader_load(&penTexFrag_, costume_frag.container, costume_frag.container_bytes))) {
        Log::logError("Xbox 360 pen shader load failed");
        releasePen();
        return false;
    }

    XsdkVertexElement penLayout[2]{};
    penLayout[0].usage = XSDK_USAGE_POSITION;
    penLayout[0].format = XSDK_VERTEX_FLOAT4;
    penLayout[1].usage = XSDK_USAGE_COLOR;
    penLayout[1].format = XSDK_VERTEX_FLOAT4;
    penLayout[1].offset_bytes = 16;
    const uint32_t penStride = sizeof(PenVertex);
    if (!NT_SUCCESS(xsdk_shader_set_vertex_layout(&penVert_, penLayout, 2, &penStride))) {
        releasePen();
        return false;
    }

    XsdkVertexElement texLayout[2]{};
    texLayout[0].usage = XSDK_USAGE_POSITION;
    texLayout[0].format = XSDK_VERTEX_FLOAT4;
    texLayout[1].usage = XSDK_USAGE_TEXCOORD;
    texLayout[1].format = XSDK_VERTEX_FLOAT2;
    texLayout[1].offset_bytes = 16;
    const uint32_t texStride = 24;
    if (!NT_SUCCESS(xsdk_shader_set_vertex_layout(&penTexVert_, texLayout, 2, &texStride))) {
        releasePen();
        return false;
    }

    XsdkSampler sampler{};
    sampler.mag_filter = sampler.min_filter = XSDK_FILTER_LINEAR;
    sampler.mip_filter = XSDK_FILTER_POINT;
    sampler.address_u = sampler.address_v = XSDK_ADDRESS_CLAMP;
    xsdk_texture_sampler_state(&penSampler_, &sampler);

    XsdkBlend blend{};
    blend.color_source = XSDK_BLEND_SRC_ALPHA;
    blend.color_dest = XSDK_BLEND_ONE_MINUS_SRC_ALPHA;
    blend.alpha_source = XSDK_BLEND_ONE;
    blend.alpha_dest = XSDK_BLEND_ONE_MINUS_SRC_ALPHA;
    blend.color_op = blend.alpha_op = XSDK_BLEND_OP_ADD;
    blend.write_mask = XSDK_WRITE_ALL;
    xsdk_gpu_blend_state(&penBlend_, &blend);

    XsdkBlend blendOpaque{};
    blendOpaque.color_source = XSDK_BLEND_ONE;
    blendOpaque.color_dest = XSDK_BLEND_ZERO;
    blendOpaque.alpha_source = XSDK_BLEND_ONE;
    blendOpaque.alpha_dest = XSDK_BLEND_ZERO;
    blendOpaque.color_op = blendOpaque.alpha_op = XSDK_BLEND_OP_ADD;
    blendOpaque.write_mask = XSDK_WRITE_ALL;
    xsdk_gpu_blend_state(&penBlendOpaque_, &blendOpaque);

    XsdkDepthStencil depth{};
    xsdk_gpu_depth_stencil_state(&penDepth_, &depth);
    XsdkRaster raster{};
    raster.cull = XSDK_CULL_NONE;
    xsdk_gpu_raster_state(&penRaster_, &raster);

    penQuadVerts_ = xsdk_gpu_alloc(6 * 6 * sizeof(float));
    penDrawVerts_ = xsdk_gpu_alloc(kMaxPenBatchVerts * sizeof(PenVertex));
    if (!penQuadVerts_ || !penDrawVerts_) {
        releasePen();
        return false;
    }

    penW_ = penW;
    penH_ = penH;
    const int projH = Scratch::projectHeight > 0 ? Scratch::projectHeight : 360;
    penScaleY_ = static_cast<float>(penH) / static_cast<float>(projH);
    penInitialized_ = true;
    penVerts_.clear();
    penReadback_.assign(size_t(penW_) * size_t(penH_) * 4, 0);
    penReadbackDirty_ = true;
    penClear();
    Log::log("Renderer_Xbox360::initPen ready: " + std::to_string(penW_) + "x" + std::to_string(penH_) +
             " hqpen=" + std::to_string(Scratch::hqpen ? 1 : 0));
    return true;
}
void Renderer_Xbox360::setRenderTarget(void *renderTarget, int width, int height) {
    if (overrideTarget_ && overrideTarget_ != renderTarget) {
        xsdk_gpu_resolve_texture(overrideTarget_, 0x00000000);
    }
    if (!renderTarget && width <= 0 && height <= 0) {
        clearRenderTarget();
        return;
    }
    if (width <= 0 || height <= 0 || width > int(XSDK_TEXTURE_MAX_SIZE) || height > int(XSDK_TEXTURE_MAX_SIZE)) {
        throw std::invalid_argument("Invalid Xbox 360 render target dimensions");
    }
    overrideTarget_ = static_cast<XsdkTexture *>(renderTarget);
    targetWidth_ = width;
    targetHeight_ = height;
    xsdk_gpu_set_target(uint32_t(width), uint32_t(height));
}
void Renderer_Xbox360::clearRenderTarget() {
    if (overrideTarget_) {
        xsdk_gpu_resolve_texture(overrideTarget_, 0x00000000);
        overrideTarget_ = nullptr;
    }
    targetWidth_ = 0;
    targetHeight_ = 0;
    xsdk_gpu_set_target(uint32_t(surface_->getWidth()), uint32_t(surface_->getHeight()));
}
bool Renderer_Xbox360::createSpeechManager() {
    if (!speechManager_) speechManager_ = new SpeechManagerXbox360();
    return speechManager_ != nullptr;
}
SpeechManager *Renderer_Xbox360::getSpeechManager() {
    if (!speechManager_) speechManager_ = new SpeechManagerXbox360();
    return speechManager_;
}
void Renderer_Xbox360::destroySpeechManager() {
    delete speechManager_;
    speechManager_ = nullptr;
}
void Renderer_Xbox360::drawRoundedBox(int w, int h, int x, int y, int radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (w <= 0 || h <= 0) return;
    radius = std::clamp(radius, 0, std::min(w, h) / 2);
    if (radius == 0) {
        drawBox(w, h, x, y, r, g, b, a);
        return;
    }
    drawBox(w, h - 2 * radius, x, y + radius, r, g, b, a);
    for (int row = 0; row < radius; ++row) {
        const double dy = radius - row - 0.5;
        const int inset = int(std::ceil(radius - std::sqrt(double(radius) * radius - dy * dy)));
        drawBox(w - 2 * inset, 1, x + inset, y + row, r, g, b, a);
        drawBox(w - 2 * inset, 1, x + inset, y + h - 1 - row, r, g, b, a);
    }
}
void Renderer_Xbox360::renderSprites() {
    if (penInitialized_ && !penVerts_.empty()) {
        flushPen();
    }
    static bool loggedFirstSprites = false;
    if (!loggedFirstSprites) {
        loggedFirstSprites = true;
        Log::log("Renderer_Xbox360::renderSprites first frame: sprites=" +
                 std::to_string(Scratch::sprites.size()) + " project=" +
                 std::to_string(Scratch::projectWidth) + "x" + std::to_string(Scratch::projectHeight));
    }
    // The runtime calls renderSprites directly; unlike menu drawing it owns the
    // complete frame, as the other single-display backends do.
    beginFrame(0, 255, 255, 255);
    if (!hasFrameBegan) return;
    for (auto it = Scratch::sprites.rbegin(); it != Scratch::sprites.rend(); ++it) {
        Sprite *sprite = *it;
        if (!sprite || !sprite->visible) continue;
        if (sprite->currentCostume < 0 || size_t(sprite->currentCostume) >= sprite->costumes.size())
            throw std::runtime_error("Invalid Sprite costume index");
        const auto &costume = sprite->costumes[sprite->currentCostume];
        auto found = Scratch::costumeImages.find(costume.fullName);
        if (found == Scratch::costumeImages.end() || !found->second) throw std::runtime_error("Missing Sprite costume: " + costume.fullName);
        auto &image = *found->second;
        sprite->spriteWidth = image.getWidth();
        sprite->spriteHeight = image.getHeight();
        sprite->spriteWidthExact = image.getExactWidth();
        sprite->spriteHeightExact = image.getExactHeight();
        Render::calculateRenderPosition(sprite, costume.isSVG);
        ImageRenderParams p;
        p.x = sprite->renderInfo.renderX;
        p.y = sprite->renderInfo.renderY;
        p.scale = sprite->renderInfo.renderScaleY;
        p.rotation = sprite->renderInfo.renderRotation;
        p.flip = sprite->rotationStyle == Sprite::LEFT_RIGHT && sprite->rotation < 0;
        p.opacity = 1 - std::clamp(sprite->ghostEffect, 0.0f, 100.0f) * 0.01f;
        p.brightness = sprite->brightnessEffect;
        p.colorEffect = sprite->colorEffect;
        p.fisheyeEffect = sprite->fisheyeEffect;
        p.whirlEffect = sprite->whirlEffect;
        p.pixelateEffect = sprite->pixelateEffect;
        p.mosaicEffect = sprite->mosaicEffect;
        image.render(p);
        if (sprite->isStage && penInitialized_) {
            renderPenLayer();
        }
    }
    // Cover drawing outside the centered project viewport before drawing UI.
    if (Scratch::projectWidth > 0 && Scratch::projectHeight > 0) {
        const double scale = std::min(double(getWidth()) / Scratch::projectWidth, double(getHeight()) / Scratch::projectHeight);
        const int left = int(std::ceil((getWidth() - Scratch::projectWidth * scale) / 2));
        const int top = int(std::ceil((getHeight() - Scratch::projectHeight * scale) / 2));
        if (left > 0) {
            drawBox(left, getHeight(), 0, 0, 0, 0, 0, 255);
            drawBox(left, getHeight(), getWidth() - left, 0, 0, 0, 0, 255);
        }
        if (top > 0) {
            drawBox(getWidth(), top, 0, 0, 0, 0, 0, 255);
            drawBox(getWidth(), top, 0, getHeight() - top, 0, 0, 0, 255);
        }
    }
    if (speechManager_) {
        speechManager_->render();
    }
    Render::renderMonitors();
    Render::renderTouchControls();
    endFrame();
}
void Renderer_Xbox360::drawPenQuad(float x, float y, float w, float h, int sw, int sh, bool opaque) {
    if (penQuadUsed_ && !xsdk_gpu_fence_done(penQuadFence_)) {
        xsdk_gpu_wait(penQuadFence_, 5000);
    }
    const float x0 = 2.0f * (x - 0.5f) / sw - 1.0f;
    const float y0 = 1.0f - 2.0f * (y - 0.5f) / sh;
    const float x1 = 2.0f * (x + w - 0.5f) / sw - 1.0f;
    const float y1 = 1.0f - 2.0f * (y + h - 0.5f) / sh;
    const float data[6][6] = {
        {x0, y0, 0, 1, 0, 0},
        {x1, y0, 0, 1, 1, 0},
        {x0, y1, 0, 1, 0, 1},
        {x0, y1, 0, 1, 0, 1},
        {x1, y0, 0, 1, 1, 0},
        {x1, y1, 0, 1, 1, 1},
    };
    for (unsigned i = 0; i < sizeof data / 4; ++i) {
        uint32_t word;
        memcpy(&word, reinterpret_cast<const uint8_t *>(data) + i * 4, 4);
        penQuadVerts_[i] = word;
    }
    __asm__ volatile("sync" ::: "memory");
    xsdk_gpu_invalidate(xsdk_gpu_address(penQuadVerts_), sizeof data);

    xsdk_gpu_set_shaders(&penTexVert_, &penTexFrag_);
    xsdk_built_shader_set_literals(&costume_frag);
    const float effects[12] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, float(penW_), float(penH_), 0.0f, 0.0f};
    xsdk_gpu_set_pixel_constants(COSTUME_FRAG_PUSH, effects, COSTUME_FRAG_PUSH_REGISTERS);
    xsdk_gpu_set_blend(opaque ? &penBlendOpaque_ : &penBlend_);
    xsdk_gpu_set_depth_stencil(&penDepth_);
    xsdk_gpu_set_raster(&penRaster_);
    xsdk_gpu_set_texture(0, &penTexture_, &penSampler_);
    xsdk_gpu_set_vertices(0, xsdk_gpu_address(penQuadVerts_), sizeof data);
    xsdk_gpu_draw(XSDK_PRIMITIVE_TRIANGLE_LIST, 6);
    penQuadFence_ = xsdk_gpu_fence();
    penQuadUsed_ = true;
}

void Renderer_Xbox360::flushPen() {
    if (!penInitialized_ || penVerts_.empty()) return;
    static unsigned flushLogs = 0;
    if (flushLogs < 8) {
        Log::log("Renderer_Xbox360::flushPen vertices=" + std::to_string(penVerts_.size()));
        ++flushLogs;
    }
    xsdk_gpu_set_target(penW_, penH_);
    drawPenQuad(0.0f, 0.0f, float(penW_), float(penH_), penW_, penH_, true);

    size_t offset = 0;
    while (offset < penVerts_.size()) {
        const size_t count = std::min(kMaxPenBatchVerts, penVerts_.size() - offset);
        if (penDrawUsed_ && !xsdk_gpu_fence_done(penDrawFence_)) {
            xsdk_gpu_wait(penDrawFence_, 5000);
        }
        const size_t bytes = count * sizeof(PenVertex);
        const uint32_t *words = reinterpret_cast<const uint32_t *>(penVerts_.data() + offset);
        for (size_t i = 0; i < bytes / 4; ++i) {
            penDrawVerts_[i] = words[i];
        }
        __asm__ volatile("sync" ::: "memory");
        xsdk_gpu_invalidate(xsdk_gpu_address(penDrawVerts_), bytes);

        xsdk_gpu_set_shaders(&penVert_, &penFrag_);
        xsdk_gpu_set_blend(&penBlend_);
        xsdk_gpu_set_depth_stencil(&penDepth_);
        xsdk_gpu_set_raster(&penRaster_);
        xsdk_gpu_set_vertices(0, xsdk_gpu_address(penDrawVerts_), uint32_t(bytes));
        xsdk_gpu_draw(XSDK_PRIMITIVE_TRIANGLE_LIST, uint32_t(count));
        penDrawFence_ = xsdk_gpu_fence();
        penDrawUsed_ = true;
        offset += count;
    }

    xsdk_gpu_resolve_texture(&penTexture_, 0x00000000);
    penVerts_.clear();
    penReadbackDirty_ = true;
}

void Renderer_Xbox360::pushPenTri(const PenVertex &a, const PenVertex &b, const PenVertex &c) {
    if (penVerts_.size() + 3 > kMaxPenBatchVerts) flushPen();
    auto toNdc = [this](PenVertex v) {
        v.x = 2.0f * (v.x - 0.5f) / penW_ - 1.0f;
        v.y = 1.0f - 2.0f * (v.y - 0.5f) / penH_;
        v.z = 0.0f;
        v.w = 1.0f;
        return v;
    };
    penVerts_.push_back(toNdc(a));
    penVerts_.push_back(toNdc(b));
    penVerts_.push_back(toNdc(c));
}

void Renderer_Xbox360::pushPenQuad(const PenVertex &v0, const PenVertex &v1, const PenVertex &v2, const PenVertex &v3) {
    pushPenTri(v0, v1, v2);
    pushPenTri(v1, v3, v2);
}

void Renderer_Xbox360::addPenCircle(float cx, float cy, float radius, float r, float g, float b, float a, unsigned segments) {
    const PenVertex center{cx, cy, 0, 1, r, g, b, a};
    for (unsigned i = 0; i < segments; ++i) {
        const float a0 = (2.0f * kPi * i) / segments;
        const float a1 = (2.0f * kPi * (i + 1)) / segments;
        pushPenTri(center,
                   {cx + std::cos(a0) * radius, cy + std::sin(a0) * radius, 0, 1, r, g, b, a},
                   {cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, 0, 1, r, g, b, a});
    }
}

void Renderer_Xbox360::renderPenLayer() {
    if (!penInitialized_) return;
    if (!hasFrameBegan) return;
    const float screenW = float(getWidth());
    const float screenH = float(getHeight());
    const float projW = float(Scratch::projectWidth > 0 ? Scratch::projectWidth : 480);
    const float projH = float(Scratch::projectHeight > 0 ? Scratch::projectHeight : 360);
    float renderX = 0.0f, renderY = 0.0f, renderW = screenW, renderH = screenH;
    if (screenW / screenH > projW / projH) {
        renderW = projW * (screenH / projH);
        renderX = (screenW - renderW) * 0.5f;
    } else {
        renderH = projH * (screenW / projW);
        renderY = (screenH - renderH) * 0.5f;
    }
    drawPenQuad(renderX, renderY, renderW, renderH, getWidth(), getHeight(), false);
}

void Renderer_Xbox360::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
    if (!penInitialized_ && !initPen()) return;
    const ColorRGBA col = CSBT2RGBA(sprite->penData.color);
    const float a = std::clamp((100.0f - sprite->penData.color.transparency) / 100.0f, 0.0f, 1.0f);
    const float r = std::clamp(col.r, 0.0f, 255.0f) / 255.0f;
    const float g = std::clamp(col.g, 0.0f, 255.0f) / 255.0f;
    const float b = std::clamp(col.b, 0.0f, 255.0f) / 255.0f;
    const float sx1 = float(x1 * penScaleY_ + penW_ * 0.5);
    const float sy1 = float(-y1 * penScaleY_ + penH_ * 0.5);
    const float sx2 = float(x2 * penScaleY_ + penW_ * 0.5);
    const float sy2 = float(-y2 * penScaleY_ + penH_ * 0.5);
    const double dx = sx2 - sx1, dy = sy2 - sy1;
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0) return;
    const float halfW = float((sprite->penData.size * 0.5) * penScaleY_);
    const float nx = float((-dy / len) * halfW);
    const float ny = float((dx / len) * halfW);
    pushPenQuad({sx1 + nx, sy1 + ny, 0, 1, r, g, b, a},
                {sx1 - nx, sy1 - ny, 0, 1, r, g, b, a},
                {sx2 + nx, sy2 + ny, 0, 1, r, g, b, a},
                {sx2 - nx, sy2 - ny, 0, 1, r, g, b, a});
}

void Renderer_Xbox360::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
    if (!penInitialized_ && !initPen()) return;
    penMoveFast(x1, y1, x2, y2, sprite);
    const ColorRGBA col = CSBT2RGBA(sprite->penData.color);
    const float a = std::clamp((100.0f - sprite->penData.color.transparency) / 100.0f, 0.0f, 1.0f);
    const float r = std::clamp(col.r, 0.0f, 255.0f) / 255.0f;
    const float g = std::clamp(col.g, 0.0f, 255.0f) / 255.0f;
    const float b = std::clamp(col.b, 0.0f, 255.0f) / 255.0f;
    const float sx1 = float(x1 * penScaleY_ + penW_ * 0.5);
    const float sy1 = float(-y1 * penScaleY_ + penH_ * 0.5);
    const float sx2 = float(x2 * penScaleY_ + penW_ * 0.5);
    const float sy2 = float(-y2 * penScaleY_ + penH_ * 0.5);
    const float radius = float((sprite->penData.size * 0.5) * penScaleY_);
    addPenCircle(sx1, sy1, radius, r, g, b, a, 12);
    addPenCircle(sx2, sy2, radius, r, g, b, a, 12);
}

void Renderer_Xbox360::penDotFast(Sprite *sprite) {
    if (!penInitialized_ && !initPen()) return;
    const ColorRGBA col = CSBT2RGBA(sprite->penData.color);
    const float a = std::clamp((100.0f - sprite->penData.color.transparency) / 100.0f, 0.0f, 1.0f);
    const float r = std::clamp(col.r, 0.0f, 255.0f) / 255.0f;
    const float g = std::clamp(col.g, 0.0f, 255.0f) / 255.0f;
    const float b = std::clamp(col.b, 0.0f, 255.0f) / 255.0f;
    const float sx = float(sprite->xPosition * penScaleY_ + penW_ * 0.5);
    const float sy = float(-sprite->yPosition * penScaleY_ + penH_ * 0.5);
    const float halfW = float((sprite->penData.size * 0.5) * penScaleY_);
    pushPenQuad({sx - halfW, sy - halfW, 0, 1, r, g, b, a},
                {sx - halfW, sy + halfW, 0, 1, r, g, b, a},
                {sx + halfW, sy - halfW, 0, 1, r, g, b, a},
                {sx + halfW, sy + halfW, 0, 1, r, g, b, a});
}

void Renderer_Xbox360::penDotAccurate(Sprite *sprite) {
    if (!penInitialized_ && !initPen()) return;
    const ColorRGBA col = CSBT2RGBA(sprite->penData.color);
    const float a = std::clamp((100.0f - sprite->penData.color.transparency) / 100.0f, 0.0f, 1.0f);
    const float r = std::clamp(col.r, 0.0f, 255.0f) / 255.0f;
    const float g = std::clamp(col.g, 0.0f, 255.0f) / 255.0f;
    const float b = std::clamp(col.b, 0.0f, 255.0f) / 255.0f;
    const float sx = float(sprite->xPosition * penScaleY_ + penW_ * 0.5);
    const float sy = float(-sprite->yPosition * penScaleY_ + penH_ * 0.5);
    const float radius = float((sprite->penData.size * 0.5) * penScaleY_);
    addPenCircle(sx, sy, radius, r, g, b, a, 16);
}

void Renderer_Xbox360::penStamp(Sprite *sprite) {
    if (!sprite || (!penInitialized_ && !initPen())) return;
    if (sprite->currentCostume < 0 || size_t(sprite->currentCostume) >= sprite->costumes.size()) return;
    const auto &costume = sprite->costumes[sprite->currentCostume];
    auto found = Scratch::costumeImages.find(costume.fullName);
    if (found == Scratch::costumeImages.end() || !found->second) return;

    flushPen();
    xsdk_gpu_set_target(penW_, penH_);
    drawPenQuad(0.0f, 0.0f, float(penW_), float(penH_), penW_, penH_, true);

    const bool prevFrameBegan = hasFrameBegan;
    targetWidth_ = penW_;
    targetHeight_ = penH_;
    hasFrameBegan = true;

    Render::calculateRenderPosition(sprite, costume.isSVG);
    const auto coords = Scratch::screenToScratchCoords(sprite->renderInfo.renderX, sprite->renderInfo.renderY, penW_, penH_);
    const float projW = float(Scratch::projectWidth > 0 ? Scratch::projectWidth : 480);
    const float projH = float(Scratch::projectHeight > 0 ? Scratch::projectHeight : 360);
    float penX = coords.first + projW * 0.5f;
    float penY = -coords.second + projH * 0.5f;
    float penScale;
    if (Scratch::hqpen) {
        penX *= penScaleY_;
        penY *= penScaleY_;
        penScale = sprite->renderInfo.renderScaleY;
    } else {
        penScale = (sprite->size / 100.0f) / costume.bitmapResolution;
        if (Scratch::bitmapHalfQuality && !costume.isSVG && costume.bitmapResolution == 2) {
            penScale *= 2.0f;
        }
    }

    ImageRenderParams p;
    p.x = penX;
    p.y = penY;
    p.scale = penScale;
    p.rotation = sprite->renderInfo.renderRotation;
    p.flip = sprite->rotationStyle == Sprite::LEFT_RIGHT && sprite->rotation < 0;
    p.opacity = 1.0f - std::clamp(sprite->ghostEffect, 0.0f, 100.0f) * 0.01f;
    p.brightness = sprite->brightnessEffect;
    p.colorEffect = sprite->colorEffect;
    p.fisheyeEffect = sprite->fisheyeEffect;
    p.whirlEffect = sprite->whirlEffect;
    p.pixelateEffect = sprite->pixelateEffect;
    p.mosaicEffect = sprite->mosaicEffect;
    found->second->render(p);
    static unsigned stampLogs = 0;
    if (stampLogs < 8) {
        Log::log("Renderer_Xbox360::penStamp costume=" + costume.fullName +
                 " x=" + std::to_string(int(penX)) + " y=" + std::to_string(int(penY)));
        ++stampLogs;
    }

    targetWidth_ = 0;
    targetHeight_ = 0;
    hasFrameBegan = prevFrameBegan;
    xsdk_gpu_resolve_texture(&penTexture_, 0x00000000);
    penReadbackDirty_ = true;
}

void Renderer_Xbox360::penClear() {
    penVerts_.clear();
    if (!penInitialized_) return;
    Log::log("Renderer_Xbox360::penClear");
    xsdk_gpu_set_target(penW_, penH_);
    xsdk_gpu_resolve_texture(&penTexture_, 0x00000000);
    penReadbackDirty_ = true;
}

void Renderer_Xbox360::preparePenColorQuery(float minX, float minY, float maxX, float maxY) {
    if (!penInitialized_ || penReadback_.empty()) return;
    if (!penVerts_.empty()) flushPen();

    const int x0 = std::clamp(int(std::floor(minX * penScaleY_ + penW_ * 0.5f)), 0, penW_ - 1);
    const int x1 = std::clamp(int(std::ceil(maxX * penScaleY_ + penW_ * 0.5f)), 0, penW_ - 1);
    const int y0 = std::clamp(int(std::floor(-maxY * penScaleY_ + penH_ * 0.5f)), 0, penH_ - 1);
    const int y1 = std::clamp(int(std::ceil(-minY * penScaleY_ + penH_ * 0.5f)), 0, penH_ - 1);
    if (x1 < x0 || y1 < y0) {
        penReadbackRowStart_ = 0;
        penReadbackRowEnd_ = -1;
        return;
    }
    if (!penReadbackDirty_ && y0 >= penReadbackRowStart_ && y1 <= penReadbackRowEnd_) return;
    if (!NT_SUCCESS(xsdk_gpu_wait(xsdk_gpu_fence(), 5000))) {
        Log::logWarning("Pen readback GPU wait failed");
        return;
    }
    const uint32_t rowCount = uint32_t(y1 - y0 + 1);
    const uint32_t rowBytes = uint32_t(penW_) * 4;
    xsdk_texture_download_rows(&penTexture_, 0, uint32_t(y0), rowCount,
                               penReadback_.data() + size_t(y0) * rowBytes, rowBytes);
    penReadbackRowStart_ = y0;
    penReadbackRowEnd_ = y1;
    penReadbackDirty_ = false;
}

bool Renderer_Xbox360::samplePenColor(float x, float y, uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &a) {
    if (!penInitialized_ || penReadback_.empty()) return false;
    const int px = int(std::floor(x * penScaleY_ + penW_ * 0.5));
    const int py = int(std::floor(-y * penScaleY_ + penH_ * 0.5));
    if (px < 0 || py < 0 || px >= penW_ || py >= penH_) return false;
    if (py < penReadbackRowStart_ || py > penReadbackRowEnd_) return false;

    const uint8_t *pixel = penReadback_.data() + (size_t(py) * size_t(penW_) + size_t(px)) * 4;
    r = pixel[0];
    g = pixel[1];
    b = pixel[2];
    a = pixel[3];
    return a != 0;
}
