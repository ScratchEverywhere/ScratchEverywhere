#include "render.hpp"
#include "image.hpp"
#include <algorithm>
#include <audio.hpp>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <downloader.hpp>
#include <image.hpp>
#include <math.hpp>
#include <render.hpp>
#include <runtime.hpp>
#include <SDL3/SDL.h>
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

Render::RenderModes Render::renderMode = Render::TOP_SCREEN_ONLY;
bool Render::hasFrameBegan;
std::vector<Monitor> Render::visibleVariables;
std::chrono::system_clock::time_point Render::startTime = std::chrono::system_clock::now();
std::chrono::system_clock::time_point Render::endTime = std::chrono::system_clock::now();
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

    debugMode = true;

    return true;
}
void Render::deInit() {
    if (penTexture != nullptr) SDL_DestroyTexture(penTexture);

    Image::cleanupImages();
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
    const auto &imgFind = images.find(sprite->costumes[sprite->currentCostume].id);
    if (imgFind == images.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return;
    }
    imgFind->second->freeTimer = imgFind->second->maxFreeTime;

    SDL_SetRenderTarget(renderer, penTexture);

    // IDK if these are needed
    sprite->rotationCenterX = sprite->costumes[sprite->currentCostume].rotationCenterX;
    sprite->rotationCenterY = sprite->costumes[sprite->currentCostume].rotationCenterY;

    // TODO: remove duplicate code (maybe make a Render::drawSprite function.)
    SDL_Image *image = imgFind->second;
    image->freeTimer = image->maxFreeTime;
    sprite->rotationCenterX = sprite->costumes[sprite->currentCostume].rotationCenterX;
    sprite->rotationCenterY = sprite->costumes[sprite->currentCostume].rotationCenterY;
    sprite->spriteWidth = static_cast<unsigned int>(image->textureRect.w) >> 1;
    sprite->spriteHeight = static_cast<unsigned int>(image->textureRect.h) >> 1;
    SDL_FlipMode flip = SDL_FLIP_NONE;
    const bool isSVG = sprite->costumes[sprite->currentCostume].isSVG;
    Render::calculateRenderPosition(sprite, isSVG);
    image->renderRect.x = sprite->renderInfo.renderX;
    image->renderRect.y = sprite->renderInfo.renderY;

    if (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0) {
        flip = SDL_FLIP_HORIZONTAL;
    }

    // Pen mapping stuff
    const auto &cords = Scratch::screenToScratchCoords(image->renderRect.x, image->renderRect.y, getWidth(), getHeight());
    image->renderRect.x = cords.first + Scratch::projectWidth / 2;
    image->renderRect.y = -cords.second + Scratch::projectHeight / 2;

    if (Scratch::hqpen) {
        image->setScale(sprite->renderInfo.renderScaleY);

        float penWidth;
        float penHeight;
        SDL_GetTextureSize(penTexture, &penWidth, &penHeight);
        const double scale = (penTexture->h / static_cast<double>(Scratch::projectHeight));

        image->renderRect.x *= scale;
        image->renderRect.y *= scale;
    } else {
        image->setScale(sprite->size / (isSVG ? 100.0f : 200.0f));
    }

    // set ghost effect
    float ghost = std::clamp(sprite->ghostEffect, 0.0f, 100.0f);
    Uint8 alpha = static_cast<Uint8>(255 * (1.0f - ghost / 100.0f));
    SDL_SetTextureAlphaMod(image->spriteTexture, alpha);

    // set brightness effect
    if (sprite->brightnessEffect != 0) {
        float brightness = sprite->brightnessEffect * 0.01f;
        if (brightness > 0.0f) {
            // render the normal image first
            SDL_RenderTextureRotated(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                                     Math::radiansToDegrees(sprite->renderInfo.renderRotation), nullptr, flip);

            // render another, blended image on top
            SDL_SetTextureBlendMode(image->spriteTexture, SDL_BLENDMODE_ADD);
            SDL_SetTextureAlphaMod(image->spriteTexture, (Uint8)(brightness * 255 * (alpha / 255.0f)));
            SDL_RenderTextureRotated(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                                     Math::radiansToDegrees(sprite->renderInfo.renderRotation), nullptr, flip);

            // reset for next frame
            SDL_SetTextureBlendMode(image->spriteTexture, SDL_BLENDMODE_BLEND);
        } else {
            Uint8 col = static_cast<Uint8>(255 * (1.0f + brightness));
            SDL_SetTextureColorMod(image->spriteTexture, col, col, col);

            SDL_RenderTextureRotated(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                                     Math::radiansToDegrees(sprite->renderInfo.renderRotation), nullptr, flip);
            // reset for next frame
            SDL_SetTextureColorMod(image->spriteTexture, 255, 255, 255);
        }
    } else {
        // if no brightness just render normal image
        SDL_SetTextureColorMod(image->spriteTexture, 255, 255, 255);
        SDL_RenderTextureRotated(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                                 Math::radiansToDegrees(sprite->renderInfo.renderRotation), nullptr, flip);
    }

    SDL_SetRenderTarget(renderer, NULL);
}

void Render::penClear() {
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
    if (shouldFlush) Image::FlushImages();
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

    // Sort sprites by layer with stage always being first
    std::vector<Sprite *> spritesByLayer = Scratch::sprites;
    std::sort(spritesByLayer.begin(), spritesByLayer.end(),
              [](const Sprite *a, const Sprite *b) {
                  // Stage sprite always comes first
                  if (a->isStage && !b->isStage) return true;
                  if (!a->isStage && b->isStage) return false;
                  // Otherwise sort by layer
                  return a->layer < b->layer;
              });

    for (Sprite *currentSprite : spritesByLayer) {

        auto imgFind = images.find(currentSprite->costumes[currentSprite->currentCostume].id);
        if (imgFind != images.end()) {
            SDL_Image *image = imgFind->second;

            currentSprite->rotationCenterX = currentSprite->costumes[currentSprite->currentCostume].rotationCenterX;
            currentSprite->rotationCenterY = currentSprite->costumes[currentSprite->currentCostume].rotationCenterY;
            currentSprite->spriteWidth = image->textureRect.w / 2;
            currentSprite->spriteHeight = image->textureRect.h / 2;

            if (!currentSprite->visible) continue;

            image->freeTimer = image->maxFreeTime;

            SDL_FlipMode flip = SDL_FLIP_NONE;
            const bool isSVG = currentSprite->costumes[currentSprite->currentCostume].isSVG;
            calculateRenderPosition(currentSprite, isSVG);
            image->renderRect.x = currentSprite->renderInfo.renderX;
            image->renderRect.y = currentSprite->renderInfo.renderY;

            image->setScale(currentSprite->renderInfo.renderScaleY);
            if (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT && currentSprite->rotation < 0) {
                flip = SDL_FLIP_HORIZONTAL;
            }

            // set ghost effect
            float ghost = std::clamp(currentSprite->ghostEffect, 0.0f, 100.0f);
            Uint8 alpha = static_cast<Uint8>(255 * (1.0f - ghost / 100.0f));
            SDL_SetTextureAlphaMod(image->spriteTexture, alpha);

            // set brightness effect
            if (currentSprite->brightnessEffect != 0) {
                float brightness = currentSprite->brightnessEffect * 0.01f;
                if (brightness > 0.0f) {
                    // render the normal image first
                    SDL_RenderTextureRotated(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                                             Math::radiansToDegrees(currentSprite->renderInfo.renderRotation), nullptr, flip);

                    // render another, blended image on top
                    SDL_SetTextureBlendMode(image->spriteTexture, SDL_BLENDMODE_ADD);
                    SDL_SetTextureAlphaMod(image->spriteTexture, (Uint8)(brightness * 255 * (alpha / 255.0f)));
                    SDL_RenderTextureRotated(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                                             Math::radiansToDegrees(currentSprite->renderInfo.renderRotation), nullptr, flip);

                    // reset for next frame
                    SDL_SetTextureBlendMode(image->spriteTexture, SDL_BLENDMODE_BLEND);
                } else {
                    Uint8 col = static_cast<Uint8>(255 * (1.0f + brightness));
                    SDL_SetTextureColorMod(image->spriteTexture, col, col, col);

                    SDL_RenderTextureRotated(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                                             Math::radiansToDegrees(currentSprite->renderInfo.renderRotation), nullptr, flip);
                    // reset for next frame
                    SDL_SetTextureColorMod(image->spriteTexture, 255, 255, 255);
                }
            } else {
                // if no brightness just render normal image
                SDL_SetTextureColorMod(image->spriteTexture, 255, 255, 255);
                SDL_RenderTextureRotated(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                                         Math::radiansToDegrees(currentSprite->renderInfo.renderRotation), nullptr, flip);
            }
        }

        // Draw collision points (for debugging)
        // std::vector<std::pair<double, double>> collisionPoints = Scratch::getCollisionPoints(currentSprite);
        // SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black points

        // for (const auto &point : collisionPoints) {
        //     double screenX = (point.first * renderScale) + (windowWidth / 2);
        //     double screenY = (point.second * -renderScale) + (windowHeight / 2);

        //     SDL_FRect debugPointRect;
        //     debugPointRect.x = static_cast<int>(screenX - renderScale); // center it a bit
        //     debugPointRect.y = static_cast<int>(screenY - renderScale);
        //     debugPointRect.w = static_cast<int>(6 * renderScale);
        //     debugPointRect.h = static_cast<int>(6 * renderScale);

        //     SDL_RenderFillRect(renderer, &debugPointRect);
        // }

        if (currentSprite->isStage) renderPenLayer();
    }

    drawBlackBars(getWidth(), getHeight());
    renderVisibleVariables();

    SDL_RenderPresent(renderer);
    Image::FlushImages();
    SoundPlayer::flushAudio();
}

std::unordered_map<std::string, std::pair<TextObject *, TextObject *>> Render::monitorTexts;
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
