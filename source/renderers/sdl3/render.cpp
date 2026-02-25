#include "render.hpp"
#include "speech_manager_sdl3.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <audio.hpp>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <downloader.hpp>
#include <image.hpp>
#include <image_sdl3.hpp>
#include <input.hpp>
#include <math.hpp>
#include <render.hpp>
#include <runtime.hpp>
#include <sprite.hpp>
#include <string>
#include <text.hpp>
#include <unordered_map>
#include <vector>
#include <window.hpp>
#include <windowing/sdl3/window.hpp>

#ifdef SYSTEM_LIBS
#include <SDL3_gfx/SDL3_gfxPrimitives.h>
#else
#include <SDL3_gfxPrimitives.h>
#endif

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

Window *globalWindow = nullptr;
SDL_Renderer *renderer = nullptr;
SDL_Texture *penTexture = nullptr;
SpeechManagerSDL3 *speechManager = nullptr;

Render::RenderModes Render::renderMode = Render::TOP_SCREEN_ONLY;
bool Render::hasFrameBegan;
std::unordered_map<std::string, Monitor> Render::visibleVariables;
bool Render::debugMode = false;
float Render::renderScale = 1.0f;

bool Render::Init() {
#ifdef __SWITCH__
    int windowWidth = 1280;
    int windowHeight = 720;
#elif defined(VITA)
    int windowWidth = 960;
    int windowHeight = 544;
#else
    int windowWidth = 540;
    int windowHeight = 405;
#endif

    TTF_Init();

    globalWindow = new WindowSDL3();
    if (!globalWindow->init(windowWidth, windowHeight, "Scratch Everywhere!")) {
        delete globalWindow;
        globalWindow = nullptr;
        return false;
    }

    renderer = SDL_CreateRenderer((SDL_Window *)globalWindow->getHandle(), "");
    if (renderer == NULL) {
        Log::logError("Could not create renderer: " + std::string(SDL_GetError()));
        return false;
    }

    debugMode = true;

    return true;
}
void Render::deInit() {
    if (speechManager) {
        delete speechManager;
        speechManager = nullptr;
    }
    if (penTexture != nullptr) SDL_DestroyTexture(penTexture);

    SoundPlayer::cleanupAudio();
    TextObject::cleanupText();
    SDL_DestroyRenderer(renderer);

    if (globalWindow) {
        globalWindow->cleanup();
        delete globalWindow;
        globalWindow = nullptr;
    }

    SoundPlayer::deinit();
    SDL_Quit();
}

void *Render::getRenderer() {
    return static_cast<void *>(renderer);
}

SpeechManager *Render::getSpeechManager() {
    if (speechManager == nullptr) speechManager = new SpeechManagerSDL3(renderer);
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
    if (penTexture != nullptr) return true;

    if (Scratch::hqpen) {
        if (Scratch::projectWidth / static_cast<double>(getWidth()) < Scratch::projectHeight / static_cast<double>(getHeight()))
            penTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, Scratch::projectWidth * (getHeight() / static_cast<double>(Scratch::projectHeight)), getHeight());
        else
            penTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, getWidth(), Scratch::projectHeight * (getWidth() / static_cast<double>(Scratch::projectWidth)));
    } else penTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, Scratch::projectWidth, Scratch::projectHeight);

    // Clear the texture
    SDL_SetTextureBlendMode(penTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(penTexture, SDL_SCALEMODE_NEAREST);
    SDL_SetRenderTarget(renderer, penTexture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, nullptr);

    return true;
}

void Render::penMove(double x1, double y1, double x2, double y2, Sprite *sprite) {
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);

    SDL_Texture *tempTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, penTexture->w, penTexture->h);
    SDL_SetTextureBlendMode(tempTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tempTexture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureAlphaMod(tempTexture, (100 - sprite->penData.color.transparency) / 100.0f * 255);
    SDL_SetRenderTarget(renderer, tempTexture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    const double scale = (penTexture->h / static_cast<double>(Scratch::projectHeight));

    const double dx = x2 * scale - x1 * scale;
    const double dy = y2 * scale - y1 * scale;

    const double length = sqrt(dx * dx + dy * dy);
    const double drawWidth = (sprite->penData.size / 2.0f) * scale;

    if (length > 0) {
        const double nx = dx / length;
        const double ny = dy / length;

        int16_t vx[4], vy[4];
        vx[0] = static_cast<int16_t>(x1 * scale + penTexture->w / 2.0f - ny * drawWidth);
        vy[0] = static_cast<int16_t>(-y1 * scale + penTexture->h / 2.0f + nx * drawWidth);
        vx[1] = static_cast<int16_t>(x1 * scale + penTexture->w / 2.0f + ny * drawWidth);
        vy[1] = static_cast<int16_t>(-y1 * scale + penTexture->h / 2.0f - nx * drawWidth);
        vx[2] = static_cast<int16_t>(x2 * scale + penTexture->w / 2.0f + ny * drawWidth);
        vy[2] = static_cast<int16_t>(-y2 * scale + penTexture->h / 2.0f - nx * drawWidth);
        vx[3] = static_cast<int16_t>(x2 * scale + penTexture->w / 2.0 - ny * drawWidth);
        vy[3] = static_cast<int16_t>(-y2 * scale + penTexture->h / 2.0f + nx * drawWidth);

        filledPolygonRGBA(renderer, vx, vy, 4, rgbColor.r, rgbColor.g, rgbColor.b, 255);
    }

    filledCircleRGBA(renderer, x1 * scale + penTexture->w / 2.0f, -y1 * scale + penTexture->h / 2.0f, drawWidth, rgbColor.r, rgbColor.g, rgbColor.b, 255);
    filledCircleRGBA(renderer, x2 * scale + penTexture->w / 2.0f, -y2 * scale + penTexture->h / 2.0f, drawWidth, rgbColor.r, rgbColor.g, rgbColor.b, 255);

    SDL_SetRenderTarget(renderer, penTexture);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(renderer, tempTexture, NULL, NULL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_DestroyTexture(tempTexture);
}

void Render::penDot(Sprite *sprite) {
    SDL_Texture *tempTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, penTexture->w, penTexture->h);
    SDL_SetTextureBlendMode(tempTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tempTexture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureAlphaMod(tempTexture, (100 - sprite->penData.color.transparency) / 100.0f * 255);
    SDL_SetRenderTarget(renderer, tempTexture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    const double scale = (penTexture->h / static_cast<double>(Scratch::projectHeight));

    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    filledCircleRGBA(renderer, sprite->xPosition * scale + penTexture->w / 2.0f, -sprite->yPosition * scale + penTexture->h / 2.0f, (sprite->penData.size / 2.0f) * scale, rgbColor.r, rgbColor.g, rgbColor.b, 255);

    SDL_SetRenderTarget(renderer, penTexture);
    SDL_RenderTexture(renderer, tempTexture, NULL, NULL);
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_DestroyTexture(tempTexture);
}

void Render::penStamp(Sprite *sprite) {
    auto imgFind = Scratch::costumeImages.find(sprite->costumes[sprite->currentCostume].fullName);
    if (imgFind == Scratch::costumeImages.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return;
    }

    SDL_SetRenderTarget(renderer, penTexture);

    Image_SDL3 *image = reinterpret_cast<Image_SDL3 *>(imgFind->second.get());

    const bool isSVG = sprite->costumes[sprite->currentCostume].isSVG;
    calculateRenderPosition(sprite, isSVG);

    // Pen mapping stuff
    const auto &cords = Scratch::screenToScratchCoords(sprite->renderInfo.renderX, sprite->renderInfo.renderY, getWidth(), getHeight());
    int penX = cords.first + Scratch::projectWidth / 2;
    int penY = -cords.second + Scratch::projectHeight / 2;

    float penScale;
    if (Scratch::hqpen) {
        float penWidth;
        float penHeight;
        SDL_GetTextureSize(image->texture, &penWidth, &penHeight);
        const double scale = (penHeight / static_cast<double>(Scratch::projectHeight));

        penX *= scale;
        penY *= scale;
        penScale = sprite->renderInfo.renderScaleY;
    } else {
        penScale = sprite->size / 100.0f;
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

    SDL_SetRenderTarget(renderer, NULL);
}

void Render::penClear() {
    if (!penTexture || penTexture == nullptr) return;
    SDL_SetRenderTarget(renderer, penTexture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, NULL);
}

void Render::beginFrame(int screen, int colorR, int colorG, int colorB) {
    if (!hasFrameBegan) {
        SDL_SetRenderDrawColor(renderer, colorR, colorG, colorB, 255);
        SDL_RenderClear(renderer);
        hasFrameBegan = true;
    }
}

void Render::endFrame(bool shouldFlush) {
    SDL_RenderPresent(renderer);
    SDL_Delay(16);
    hasFrameBegan = false;
}

void Render::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    SDL_SetRenderDrawColor(renderer, colorR, colorG, colorB, colorA);
    SDL_FRect rect = {x - (w / 2.0f), y - (h / 2.0f), static_cast<float>(w), static_cast<float>(h)};
    SDL_RenderFillRect(renderer, &rect);
}

void drawBlackBars(int screenWidth, int screenHeight) {
    float screenAspect = static_cast<float>(screenWidth) / screenHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;

    if (screenAspect > projectAspect) {
        // Vertical bars,,,
        float scale = static_cast<float>(screenHeight) / Scratch::projectHeight;
        float scaledProjectWidth = Scratch::projectWidth * scale;
        float barWidth = (screenWidth - scaledProjectWidth) / 2.0f;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_FRect leftBar = {0, 0, std::ceil(barWidth), static_cast<float>(screenHeight)};
        SDL_FRect rightBar = {std::floor(screenWidth - barWidth), 0, std::ceil(barWidth), static_cast<float>(screenHeight)};

        SDL_RenderFillRect(renderer, &leftBar);
        SDL_RenderFillRect(renderer, &rightBar);
    } else if (screenAspect < projectAspect) {
        // Horizontal bars,,,
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

void Render::renderSprites() {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    for (auto it = Scratch::sprites.rbegin(); it != Scratch::sprites.rend(); ++it) {
        Sprite *currentSprite = *it;
        auto imgFind = Scratch::costumeImages.find(currentSprite->costumes[currentSprite->currentCostume].fullName);
        if (imgFind != Scratch::costumeImages.end()) {
            Image *image = imgFind->second.get();

            const bool isSVG = currentSprite->costumes[currentSprite->currentCostume].isSVG;
            calculateRenderPosition(currentSprite, isSVG);
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
        // Draw collision points (for debugging)
        // std::vector<std::pair<double, double>> collisionPoints = Scratch::getCollisionPoints(currentSprite);
        // SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black points

        // for (const auto &point : collisionPoints) {
        //     double screenX = (point.first * renderScale) + (getWidth() / 2);
        //     double screenY = (point.second * -renderScale) + (getHeight() / 2);

        //     SDL_FRect debugPointRect;
        //     debugPointRect.x = screenX - renderScale; // center it a bit
        //     debugPointRect.y = screenY - renderScale;
        //     debugPointRect.w = 2 * renderScale;
        //     debugPointRect.h = 2 * renderScale;

        //     SDL_RenderFillRect(renderer, &debugPointRect);
        // }

        if (currentSprite->isStage) renderPenLayer();
    }

    if (speechManager) {
        speechManager->render();
    }

    drawBlackBars(getWidth(), getHeight());
    renderVisibleVariables();

#if !defined(PLATFORM_HAS_MOUSE) && !defined(PLATFORM_HAS_TOUCH)
    if (Input::mousePointer.isMoving) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_FRect rect;
        rect.w = rect.h = 5;
        rect.x = (Input::mousePointer.x * renderScale) + (globalWindow->getWidth() * 0.5);
        rect.y = (Input::mousePointer.y * -1 * renderScale) + (globalWindow->getHeight() * 0.5);
        Input::mousePointer.x = std::clamp((float)Input::mousePointer.x, -Scratch::projectWidth * 0.5f, Scratch::projectWidth * 0.5f);
        Input::mousePointer.y = std::clamp((float)Input::mousePointer.y, -Scratch::projectHeight * 0.5f, Scratch::projectHeight * 0.5f);
        SDL_RenderRect(renderer, &rect);
    }
#endif

    SDL_RenderPresent(renderer);
    SoundPlayer::flushAudio();
}

std::unordered_map<std::string, std::pair<std::unique_ptr<TextObject>, std::unique_ptr<TextObject>>> Render::monitorTexts;
std::unordered_map<std::string, Render::ListMonitorRenderObjects> Render::listMonitors;

void Render::renderPenLayer() {
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

    SDL_RenderTexture(renderer, penTexture, NULL, &renderRect);
}

bool Render::appShouldRun() {
    if (OS::toExit) return false;
    if (globalWindow) {
        globalWindow->pollEvents();

        static int lastW = 0, lastH = 0;
        int currentW = globalWindow->getWidth();
        int currentH = globalWindow->getHeight();

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

        return !globalWindow->shouldClose();
    }
    return false;
}
