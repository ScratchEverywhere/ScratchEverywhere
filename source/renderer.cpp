#include "renderer.hpp"
#include "settings.hpp"
#include <cmath>
#include <log.hpp>
#include <memory>
#include <pixel_snap.hpp>

#ifdef RENDERER_OPENGL_CORE
#include "renderer_gl_core.hpp"
#endif
#ifdef RENDERER_OPENGL
#include "renderer_gl.hpp"
#endif
#ifdef RENDERER_XBOX
#include "renderer_xbox.hpp"
#endif
#ifdef RENDERER_NV2A
#include "renderer_nv2a.hpp"
#endif
#ifdef RENDERER_GX2
#include "renderer_gx2.hpp"
#endif
#ifdef RENDERER_SDL3
#include "renderer_sdl3.hpp"
#endif
#ifdef RENDERER_SDL2
#include "renderer_sdl2.hpp"
#endif
#ifdef RENDERER_SDL1
#include "renderer_sdl1.hpp"
#endif
#ifdef RENDERER_GDI
#include "renderer_gdi.hpp"
#endif
#ifdef RENDERER_CITRO2D
#include "renderer_c2d.hpp"
#endif
#ifdef RENDERER_GL2D
#include "renderer_gl2d.hpp"
#endif
#ifdef RENDERER_D3D9
#include "renderer_d3d9.hpp"
#endif
#ifdef RENDERER_HEADLESS
#include "renderer_headless.hpp"
#endif

Renderer *globalRenderer = nullptr;

Renderer::Renderer() {}

namespace Render {
Renderers currentRenderer = Renderers::_count;
float renderScale = 1.0f;
RenderModes renderMode = Render::TOP_SCREEN_ONLY;

std::unordered_map<std::string, Monitor> monitors;
} // namespace Render

Renderer *Render::getGlobalRenderer() {
    return globalRenderer;
}

const std::vector<std::string> Render::getAvailableRenderers() {
    std::vector<std::string> renderers;

    auto add = [&](Renderers r) {
        uint8_t id = static_cast<uint8_t>(r);
        renderers.push_back(RendererNames[id]);
    };

#ifdef RENDERER_OPENGL_CORE
    add(Renderers::OpenGLCore);
#endif
#ifdef RENDERER_OPENGL
    add(Renderers::OpenGL);
#endif
#ifdef RENDERER_D3D9
    add(Renderers::D3D9);
#endif
#ifdef RENDERER_NV2A
    add(Renderers::NV2A);
#endif
#ifdef RENDERER_XBOX
    add(Renderers::Xbox);
#endif
#ifdef RENDERER_GX2
    add(Renderers::GX2);
#endif
#ifdef RENDERER_SDL3
    add(Renderers::SDL3);
#endif
#ifdef RENDERER_SDL2
    add(Renderers::SDL2);
#endif
#ifdef RENDERER_SDL1
    add(Renderers::SDL1);
#endif
#ifdef RENDERER_GDI
    add(Renderers::GDI);
#endif
#ifdef RENDERER_CITRO2D
    add(Renderers::Citro2D);
#endif
#ifdef RENDERER_GL2D
    add(Renderers::GL2D);
#endif
#ifdef RENDERER_HEADLESS
    add(Renderers::Headless);
#endif

    return renderers;
}

bool Render::init() {

    const auto j = SettingsManager::getConfigSettings();
    uint8_t startIndex = 0;
    bool found = false;

    // usually only a run argument would change currentRenderer at this point
    if (currentRenderer != Renderers::_count) {
        startIndex = static_cast<uint8_t>(currentRenderer);
        found = true;
    }

    if (!found && j.contains("Renderer") && j["Renderer"].is_string()) {
        std::string rendererString = j["Renderer"].get<std::string>();
#if defined(RENDERER_XBOX) && !defined(RENDERER_SDL3)
        // Settings saved before the dedicated Xbox renderer existed still say "SDL3".
        if (rendererString == "SDL3") rendererString = "Xbox";
#endif
        for (auto &r : RendererNames) {
            if (r == rendererString) {
                Log::log("Initializing " + rendererString + " renderer from settings...");
                found = true;
                break;
            }
            startIndex++;
        }
    }
    if (!found) {
        Log::logWarning("No renderer found in settings, initializing default...");
        startIndex = 0;
    }

    uint8_t totalBackends = static_cast<uint8_t>(Renderers::_count);
    for (uint8_t step = 0; step < totalBackends; ++step) {
        uint8_t i = (startIndex + step) % totalBackends;
        auto rendererName = static_cast<Renderers>(i);

        switch (rendererName) {
        case Renderers::OpenGLCore: {
#ifdef RENDERER_OPENGL_CORE
            Render::currentRenderer = Renderers::OpenGLCore;
            globalRenderer = new Renderer_GLCore;
            break;
#else
            continue;
#endif
        }
        case Renderers::OpenGL: {
#ifdef RENDERER_OPENGL
            Render::currentRenderer = Renderers::OpenGL;
            globalRenderer = new Renderer_GL;
            break;
#else
            continue;
#endif
        }
        case Renderers::D3D9: {
#ifdef RENDERER_D3D9
            Render::currentRenderer = Renderers::D3D9;
            globalRenderer = new Renderer_D3D9;
            break;
#else
            continue;
#endif
        }
        case Renderers::NV2A: {
#ifdef RENDERER_NV2A
            Render::currentRenderer = Renderers::NV2A;
            globalRenderer = new Renderer_NV2A;
            break;
#else
            continue;
#endif
        }
        case Renderers::Xbox: {
#ifdef RENDERER_XBOX
            Render::currentRenderer = Renderers::Xbox;
            globalRenderer = new Renderer_Xbox;
            break;
#else
            continue;
#endif
        }
        case Renderers::GX2: {
#ifdef RENDERER_GX2
            Render::currentRenderer = Renderers::GX2;
            globalRenderer = new Renderer_GX2;
            break;
#else
            continue;
#endif
        }
        case Renderers::SDL3: {
#ifdef RENDERER_SDL3
            Render::currentRenderer = Renderers::SDL3;
            globalRenderer = new Renderer_SDL3;
            break;
#else
            continue;
#endif
        }
        case Renderers::SDL2: {
#ifdef RENDERER_SDL2
            Render::currentRenderer = Renderers::SDL2;
            globalRenderer = new Renderer_SDL2;
            break;
#else
            continue;
#endif
        }
        case Renderers::SDL1: {
#ifdef RENDERER_SDL1
            Render::currentRenderer = Renderers::SDL1;
            globalRenderer = new Renderer_SDL1;
            break;
#else
            continue;
#endif
        }
        case Renderers::Citro2D: {
#ifdef RENDERER_CITRO2D
            Render::currentRenderer = Renderers::Citro2D;
            globalRenderer = new Renderer_C2D;
            break;
#else
            continue;
#endif
        }
        case Renderers::GL2D: {
#ifdef RENDERER_GL2D
            Render::currentRenderer = Renderers::GL2D;
            globalRenderer = new Renderer_GL2D;
            break;
#else
            continue;
#endif
        }
        case Renderers::GDI: {
#ifdef RENDERER_GDI
            Render::currentRenderer = Renderers::GDI;
            globalRenderer = new Renderer_GDI;
            break;
#else
            continue;
#endif
        }
        case Renderers::Headless: {
#ifdef RENDERER_HEADLESS
            Render::currentRenderer = Renderers::Headless;
            globalRenderer = new Renderer_Headless;
            break;
#else
            continue;
#endif
        }
        default: {
            continue;
        }
        }

        if (globalRenderer == nullptr) {
            Log::logError(std::string(RendererNames[i]) + " failed to init.");
            delete globalRenderer;
            globalRenderer = nullptr;
            continue;
        } else if (globalRenderer->error.has_value()) {
            Log::logError(std::string(RendererNames[i]) + " failed to init: " + globalRenderer->error.value());
            delete globalRenderer;
            globalRenderer = nullptr;
            continue;
        } else break;
    }

    return (globalRenderer != nullptr);
}

void Render::deInit() {
    delete globalRenderer;
}

bool Render::initPen() {
    if (globalRenderer)
        return globalRenderer->initPen();
    return false;
}

void Render::setRenderTarget(void *renderTarget, int width, int height) {
    if (globalRenderer)
        globalRenderer->setRenderTarget(renderTarget, width, height);
}

void Render::clearRenderTarget() {
    if (globalRenderer)
        globalRenderer->clearRenderTarget();
}

bool Render::createSpeechManager() {
    if (globalRenderer)
        return globalRenderer->createSpeechManager();
    return false;
}

SpeechManager *Render::getSpeechManager() {
    if (globalRenderer)
        return globalRenderer->getSpeechManager();
    return nullptr;
}

void Render::destroySpeechManager() {
    if (globalRenderer)
        globalRenderer->destroySpeechManager();
}

void Render::beginFrame(int screen, int colorR, int colorG, int colorB) {
    if (globalRenderer)
        globalRenderer->beginFrame(screen, colorR, colorG, colorB);
}

void Render::endFrame(bool shouldFlush) {
    if (globalRenderer)
        globalRenderer->endFrame(shouldFlush);
}

int Render::getWidth() {
    if (globalRenderer)
        return globalRenderer->getWidth();
    return 0;
}

int Render::getHeight() {
    if (globalRenderer)
        return globalRenderer->getHeight();
    return 0;
}

float Render::getPixelDensity() {
    if (globalRenderer)
        return globalRenderer->getPixelDensity();
    return 1.0f;
}

void Render::renderSprites() {
    if (globalRenderer)
        globalRenderer->renderSprites();
}

void Render::renderPenLayer() {
    if (globalRenderer)
        globalRenderer->renderPenLayer();
}

void Render::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    if (globalRenderer)
        globalRenderer->drawBox(w, h, x, y, colorR, colorG, colorB, colorA);
}
void Render::drawRoundedBox(int w, int h, int x, int y, int radius, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    if (globalRenderer)
        globalRenderer->drawRoundedBox(w, h, x, y, radius, colorR, colorG, colorB, colorA);
}

bool Render::appShouldRun() {
    if (globalRenderer)
        return globalRenderer->appShouldRun();
    return true; // probably
}

void Render::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
    if (globalRenderer)
        globalRenderer->penMoveFast(x1, y1, x2, y2, sprite);
}

void Render::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
    if (globalRenderer)
        globalRenderer->penMoveAccurate(x1, y1, x2, y2, sprite);
}

void Render::penDotFast(Sprite *sprite) {
    if (globalRenderer)
        globalRenderer->penDotFast(sprite);
}

void Render::penDotAccurate(Sprite *sprite) {
    if (globalRenderer)
        globalRenderer->penDotAccurate(sprite);
}

void Render::penStamp(Sprite *sprite) {
    if (globalRenderer)
        globalRenderer->penStamp(sprite);
}

void Render::penClear() {
    if (globalRenderer)
        globalRenderer->penClear();
}

void Render::cleanupPen() {
    if (globalRenderer)
        globalRenderer->cleanupPen();
}

namespace {
#if defined(__PC__)
constexpr float monitorCornerRadiusFactor = 3.0f;

void drawMonitorBox(float w, float h, float x, float y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    const int radius = static_cast<int>(monitorCornerRadiusFactor * Render::renderScale);
    Render::drawRoundedBox(static_cast<int>(w), static_cast<int>(h), static_cast<int>(x), static_cast<int>(y), radius, r, g, b, a);
}
#else
void drawMonitorBox(float w, float h, float x, float y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    Render::drawBox(static_cast<int>(w), static_cast<int>(h), static_cast<int>(x), static_cast<int>(y), r, g, b, a);
}
#endif
} // namespace

void Render::calculateRenderPosition(Sprite *sprite, const bool isSVG) {
    const int screenWidth = getWidth();
    const int screenHeight = getHeight();
    const Costume &costume = sprite->costumes[sprite->currentCostume];

    // If the window size changed, or if the sprite changed costumes
    if (sprite->renderInfo.forceUpdate || sprite->currentCostume != sprite->renderInfo.oldCostumeID) {
#ifdef __XBOX__
        sprite->renderInfo.oldX = -999999.0f;
        sprite->renderInfo.oldY = -999999.0f;
        sprite->renderInfo.oldRotation = -999999.0f;
        sprite->renderInfo.oldSize = -999999.0f;
#else
        // change all renderinfo a bit to update position for all
        sprite->renderInfo.oldX++;
        sprite->renderInfo.oldY++;
        sprite->renderInfo.oldRotation++;
        sprite->renderInfo.oldSize++;
#endif
        sprite->renderInfo.oldCostumeID = sprite->currentCostume;
        sprite->renderInfo.forceUpdate = false;
    }

    if (sprite->size != sprite->renderInfo.oldSize) {
        sprite->renderInfo.oldSize = sprite->size;
#ifdef __XBOX__
        sprite->renderInfo.oldRotation = -999999.0f;
        sprite->renderInfo.oldX = -999999.0f;
        sprite->renderInfo.oldY = -999999.0f;
#else
        sprite->renderInfo.oldRotation++;
        sprite->renderInfo.oldX++;
        sprite->renderInfo.oldY++;
#endif
        sprite->renderInfo.renderScaleX = (sprite->size * 0.01f) / costume.bitmapResolution;

        if (renderMode != BOTH_SCREENS && screenHeight != Scratch::projectHeight) {
            float scale = std::min(static_cast<float>(screenWidth) / Scratch::projectWidth, static_cast<float>(screenHeight) / Scratch::projectHeight);
            sprite->renderInfo.renderScaleX *= scale;
        }
        if (Scratch::bitmapHalfQuality && !isSVG && costume.bitmapResolution == 2) {
            sprite->renderInfo.renderScaleX *= 2;
        }
        sprite->renderInfo.renderScaleY = sprite->renderInfo.renderScaleX;
    }
    if (sprite->rotation != sprite->renderInfo.oldRotation) {
        sprite->renderInfo.oldRotation = sprite->rotation;
#ifdef __XBOX__
        sprite->renderInfo.oldSize = -999999.0f;
        sprite->renderInfo.oldX = -999999.0f;
        sprite->renderInfo.oldY = -999999.0f;
#else
        sprite->renderInfo.oldSize++;
        sprite->renderInfo.oldX++;
        sprite->renderInfo.oldY++;
#endif
        if (sprite->rotationStyle == sprite->ALL_AROUND) {
            sprite->renderInfo.renderRotation = Math::degreesToRadians(sprite->rotation - 90);
        } else {
            sprite->renderInfo.renderRotation = 0;
        }
    }
    if (sprite->xPosition != sprite->renderInfo.oldX ||
        sprite->yPosition != sprite->renderInfo.oldY) {

        sprite->renderInfo.oldX = sprite->xPosition;
        sprite->renderInfo.oldY = sprite->yPosition;

        float renderX;
        float renderY;
        float spriteX = std::round(sprite->xPosition);
        float spriteY = std::round(sprite->yPosition);

        float rotCenterX = costume.rotationCenterX * 2;
        float rotCenterY = costume.rotationCenterY * 2;

        const float costumeW = sprite->spriteWidthExact > 0.0f ? sprite->spriteWidthExact : static_cast<float>(sprite->spriteWidth);
        const float costumeH = sprite->spriteHeightExact > 0.0f ? sprite->spriteHeightExact : static_cast<float>(sprite->spriteHeight);

        if (costumeW - rotCenterX != 0.0f ||
            costumeH - rotCenterY != 0.0f) {

            float offsetX = (costumeW - rotCenterX) * 0.5f;
            float offsetY = (costumeH - rotCenterY) * 0.5f;

            if (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0)
                offsetX *= -1;

            // Offset based on size
#ifdef __XBOX__
            float scale = (sprite->size * 0.01f) / costume.bitmapResolution;
            if (Scratch::bitmapHalfQuality && !isSVG && costume.bitmapResolution == 2) {
                scale *= 2.0f;
            }
#else
            float scale = (sprite->size * 0.01f) / costume.bitmapResolution;
#endif
            offsetX *= scale;
            offsetY *= scale;

            // Offset based on rotation
            if (sprite->renderInfo.renderRotation != 0.0f) {
                float rotCos = cos(sprite->renderInfo.renderRotation);
                float rotSin = sin(sprite->renderInfo.renderRotation);

                float rotX = offsetX * rotCos - offsetY * rotSin;
                float rotY = offsetX * rotSin + offsetY * rotCos;

                offsetX = rotX;
                offsetY = rotY;
            }

            spriteX += offsetX;
            spriteY -= offsetY;
        }

        if (renderMode != BOTH_SCREENS && (screenWidth != Scratch::projectWidth || screenHeight != Scratch::projectHeight)) {
            renderX = (spriteX * renderScale) + (screenWidth * 0.5);
            renderY = (-spriteY * renderScale) + (screenHeight * 0.5);
        } else {
            renderX = spriteX + (screenWidth * 0.5);
            renderY = -spriteY + (screenHeight * 0.5);
        }

        sprite->renderInfo.renderX = renderX;
        sprite->renderInfo.renderY = renderY;
    }
}

void Render::setRenderScale() {
    const int screenWidth = getWidth();
    const int screenHeight = getHeight();
    renderScale = std::min(static_cast<float>(screenWidth) / Scratch::projectWidth,
                           static_cast<float>(screenHeight) / Scratch::projectHeight);
    if (renderMode == BOTH_SCREENS) renderScale = 1.0f;
    if (renderMode == BOTH_SCREENS) PixelSnap::setGrid(1.0f, 0.0f, 0.0f);
    else PixelSnap::setGrid(renderScale, screenWidth * 0.5f, screenHeight * 0.5f);
    forceUpdateSpritePosition();
}

void Render::resizeSVGs() {
    for (auto &sprite : Scratch::sprites) {
        resizeSVGs(sprite);
    }
}

void Render::resizeSVGs(Sprite *sprite) {
    const int screenWidth = getWidth();
    const int screenHeight = getHeight();

    for (auto &costume : sprite->costumes) {
        auto imgFind = Scratch::costumeImages.find(costume.fullName);
        if (imgFind == Scratch::costumeImages.end()) continue;

        float scale = sprite->size / 100;
        scale *= std::min(static_cast<float>(screenWidth) / Scratch::projectWidth, static_cast<float>(screenHeight) / Scratch::projectHeight);

        const float oldScale = imgFind->second->getScale();

        auto potentialError = imgFind->second->resizeSVG(scale);
        if (!potentialError.has_value()) Log::logWarning("Error resizing SVG: " + costume.id);

        if (imgFind->second->getScale() != oldScale) {
            costume.collisionMask = nullptr;
        }
    }
}

void Render::forceUpdateSpritePosition() {
    for (auto &sprite : Scratch::sprites) {
        sprite->renderInfo.forceUpdate = true;
    }
}

bool Render::checkFramerate() {
    static Timer frameTimer;
    int frameDuration = 1000 / Scratch::FPS;
    return frameTimer.hasElapsedAndRestart(frameDuration);
}

std::string Render::getVariableValueString(Value value) {
    if (value.isDouble()) {
        return Math::toString(std::round(value.get<double>() * 1e6) / 1e6); // js Number(value.toFixed(6))
    } else if (value.isUndefined()) {
        return ""; // Scratch keeps the original value, leave blank for now
    } else {
        return value.get<std::string>();
    }
}

std::string Render::getListValueString(Value value) {
    if (value.isUndefined()) {
        return ""; // Scratch crashes, TurboWarp shows empty string
    } else {
        return value.asString();
    }
}

ColorRGBA Render::getMonitorValueColor(const std::string &opcode) {
    if (opcode.substr(0, 5) == "data_")
        return {.r = 255, .g = 140, .b = 26, .a = 255};
    else if (opcode.substr(0, 8) == "sensing_")
        return {.r = 92, .g = 177, .b = 214, .a = 255};
    else if (opcode.substr(0, 7) == "motion_")
        return {.r = 76, .g = 151, .b = 255, .a = 255};
    else if (opcode.substr(0, 6) == "looks_")
        return {.r = 153, .g = 102, .b = 255, .a = 255};
    else if (opcode.substr(0, 6) == "sound_")
        return {.r = 207, .g = 99, .b = 207, .a = 255};
    else return {.r = 255, .g = 140, .b = 26, .a = 255};
}
