#include <algorithm>
#include <cmath>
#include <cstdio>
#include <input.hpp>
#include <log.hpp>
#include <os.hpp>
#include <renderer_xbox360.hpp>
#include <stdexcept>
#include <xsdk/gpu.h>
#include <xsdk/text.h>

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
        return;
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
int Renderer_Xbox360::getWidth() { return surface_->getWidth(); }
int Renderer_Xbox360::getHeight() { return surface_->getHeight(); }
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
bool Renderer_Xbox360::initPen() {
    Log::logWarning("Xbox 360 pen rendering is not implemented");
    return false;
}
void Renderer_Xbox360::setRenderTarget(void *, int, int) { unsupported("offscreen targets"); }
void Renderer_Xbox360::clearRenderTarget() { xsdk_gpu_set_target(getWidth(), getHeight()); }
bool Renderer_Xbox360::createSpeechManager() { return false; }
SpeechManager *Renderer_Xbox360::getSpeechManager() { return nullptr; }
void Renderer_Xbox360::destroySpeechManager() {} // No speech manager can be created by this backend yet.
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
        image.render(p);
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
    Render::renderMonitors();
    Render::renderTouchControls();
    endFrame();
}
void Renderer_Xbox360::renderPenLayer() { unsupported("pen rendering"); }
void Renderer_Xbox360::penMoveFast(double, double, double, double, Sprite *) { unsupported("pen lines"); }
void Renderer_Xbox360::penMoveAccurate(double, double, double, double, Sprite *) { unsupported("pen lines"); }
void Renderer_Xbox360::penDotFast(Sprite *) { unsupported("pen dots"); }
void Renderer_Xbox360::penDotAccurate(Sprite *) { unsupported("pen dots"); }
void Renderer_Xbox360::penStamp(Sprite *) { unsupported("pen stamps"); }
void Renderer_Xbox360::penClear() {
    // initPen() returns false, so project cleanup has no pen target to clear.
}
