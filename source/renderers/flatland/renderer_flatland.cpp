#include "renderer_flatland.hpp"
#include "flatland_backend.hpp"
#include "image_flatland.hpp"
#include "window.hpp"
#include <algorithm>
#include <image.hpp>
#include <log.hpp>
#include <math.hpp>
#include <runtime.hpp>
#include <unzip.hpp>

namespace {
constexpr float svgTextureScale = 2.0f;
} // namespace

Renderer_Flatland::Renderer_Flatland() {
    window = createWindow(0, 0, "", WindowingBackends::Flatland);
}

Renderer_Flatland::~Renderer_Flatland() {
    window->cleanup();
    delete window;
}

bool Renderer_Flatland::initPen() {
    return true;
}

void Renderer_Flatland::setRenderTarget(void *renderTarget, int width, int height) {}

void Renderer_Flatland::clearRenderTarget() {}

bool Renderer_Flatland::createSpeechManager() {
    return true;
}

SpeechManager *Renderer_Flatland::getSpeechManager() {
    return nullptr;
}

void Renderer_Flatland::destroySpeechManager() {}

void Renderer_Flatland::beginFrame(int screen, int colorR, int colorG, int colorB) {
    hasFrameBegan = true;
}

void Renderer_Flatland::endFrame(bool shouldFlush) {
    hasFrameBegan = false;
}

int Renderer_Flatland::getWidth() {
    return Scratch::projectWidth;
}

int Renderer_Flatland::getHeight() {
    return Scratch::projectHeight;
}

float Renderer_Flatland::getPixelDensity() {
    return 1.0f;
}

void Renderer_Flatland::preloadProject() {
    FlatlandBackend &backend = FlatlandBackend::get();

    for (Sprite *sprite : Scratch::sprites) {
        for (const Costume &costume : sprite->costumes) {
            const float scale = costume.isSVG ? svgTextureScale : 1.0f;
            if (Scratch::projectType == ProjectType::UNZIPPED) {
                createImageFromFile(costume.fullName, true, false, scale);
            } else {
                createImageFromZip(costume.fullName, Scratch::sb3InRam ? Unzip::zipArchive.get() : nullptr, false, scale);
            }
        }
    }

    backend.seal(Scratch::projectWidth, Scratch::projectHeight);
}

void Renderer_Flatland::renderSprites() {
    FlatlandBackend &backend = FlatlandBackend::get();
    if (!backend.isSealed()) preloadProject();

    auto frame = std::make_shared<FlatlandFrame>();
    frame->sequence = ++frameSequence;

    int32_t layer = 0;
    for (auto it = Scratch::sprites.rbegin(); it != Scratch::sprites.rend(); ++it) {
        Sprite *sprite = *it;
        const Costume &costume = sprite->costumes[sprite->currentCostume];
        auto imageFind = Scratch::costumeImages.find(costume.fullName);
        if (imageFind == Scratch::costumeImages.end()) continue;
        Image_Flatland *image = static_cast<Image_Flatland *>(imageFind->second.get());

        Render::calculateRenderPosition(sprite, costume.isSVG);
        if (!sprite->visible || image->textureId < 0 || sprite->ghostEffect >= 100.0f) continue;

        const auto &info = sprite->renderInfo;
        const bool flip = sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0;

        flatland::SpriteTransform transform;
        transform.x = info.renderX;
        transform.y = info.renderY;
        transform.width = image->getExactWidth() * info.renderScaleY * (flip ? -1.0 : 1.0);
        transform.height = image->getExactHeight() * info.renderScaleY;
        transform.rotation = Math::radiansToDegrees(info.renderRotation);
        transform.layer = ++layer;

        frame->sprites.push_back({reinterpret_cast<uintptr_t>(sprite), image->textureId, transform});
    }

    if (backend.cursorTexture() >= 0) {
        auto [cursorX, cursorY] = backend.cursorPosition();
        flatland::SpriteTransform transform;
        transform.x = cursorX;
        transform.y = cursorY;
        transform.width = transform.height = backend.cursorSize();
        transform.layer = ++layer;
        frame->sprites.push_back({0, backend.cursorTexture(), transform});
    }

    backend.publishFrame(std::move(frame));
}

void Renderer_Flatland::renderPenLayer() {}

void Renderer_Flatland::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {}

void Renderer_Flatland::drawRoundedBox(int w, int h, int x, int y, int radius, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {}

bool Renderer_Flatland::appShouldRun() {
    return true;
}

void Renderer_Flatland::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {}

void Renderer_Flatland::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {}

void Renderer_Flatland::penDotFast(Sprite *sprite) {}

void Renderer_Flatland::penDotAccurate(Sprite *sprite) {}

void Renderer_Flatland::penStamp(Sprite *sprite) {}

void Renderer_Flatland::penClear() {}
