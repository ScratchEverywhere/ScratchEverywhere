#include "../scratch/render.hpp"
#include "../scratch/image.hpp"
#include "audio.hpp"
#include "blocks/pen.hpp"
#include "downloader.hpp"
#include "image.hpp"
#include "interpret.hpp"
#include "math.hpp"
#include "render.hpp"
#include "sprite.hpp"
#include "text.hpp"
#include "unzip.hpp"
#include <SDL/SDL.h>
#include <SDL/SDL_gfxBlitFunc.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_rotozoom.h>
#include <SDL/SDL_video.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef __MINGW32__
#define filledCircleRGBA GFX_filledCircleRGBA
#define filledPolygonRGBA GFX_filledPolygonRGBA
#endif

int windowWidth = 540;
int windowHeight = 405;
SDL_Surface *window = nullptr;
SDL_Surface *penSurface = nullptr;

Render::RenderModes Render::renderMode = Render::TOP_SCREEN_ONLY;
bool Render::hasFrameBegan;
std::vector<Monitor> Render::visibleVariables;
std::chrono::system_clock::time_point Render::startTime = std::chrono::system_clock::now();
std::chrono::system_clock::time_point Render::endTime = std::chrono::system_clock::now();
bool Render::debugMode = false;
float Render::renderScale = 1.0f;

// TODO: properly export these to input.cpp
SDL_Joystick *controller;
bool touchActive = false;
SDL_Rect touchPosition;

bool Render::Init() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
    SDL_EnableUNICODE(1);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    TTF_Init();
    SDL_WM_SetCaption("Scratch Everywhere!", NULL);
    window = SDL_SetVideoMode(windowWidth, windowHeight, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_RESIZABLE);
    if (!window) {
        Log::logError("SDL_SetVideoMode failed: %s", SDL_GetError());
        return false;
    }

    if (SDL_NumJoysticks() > 0) controller = SDL_JoystickOpen(0);

    debugMode = true;

    // Print SDL version number. could be useful for debugging
    SDL_version ver;
    SDL_VERSION(&ver);
    Log::log("SDL v" + std::to_string(ver.major) + "." + std::to_string(ver.minor) + "." + std::to_string(ver.patch));

    socketInitializeDefault();
    return true;
}
void Render::deInit() {
    SDL_FreeSurface(penSurface);

    Image::cleanupImages();
    SoundPlayer::cleanupAudio();
    TextObject::cleanupText();
    SoundPlayer::deinit();
    IMG_Quit();
    SDL_Quit();
}

void *Render::getRenderer() {
    return static_cast<void *>(window);
}

int Render::getWidth() {
    return windowWidth;
}
int Render::getHeight() {
    return windowHeight;
}

bool Render::initPen() {
    if (penSurface != nullptr) return true;

    if (Scratch::hqpen) {
        if (Scratch::projectWidth / static_cast<double>(windowWidth) < Scratch::projectHeight / static_cast<double>(windowHeight))
            penSurface = SDL_CreateRGBSurface(SDL_HWSURFACE, Scratch::projectWidth * (windowHeight / static_cast<double>(Scratch::projectHeight)), windowHeight, 32, RMASK, GMASK, BMASK, AMASK);
        else
            penSurface = SDL_CreateRGBSurface(SDL_HWSURFACE, windowWidth, Scratch::projectHeight * (windowWidth / static_cast<double>(Scratch::projectWidth)), 32, RMASK, GMASK, BMASK, AMASK);
    } else penSurface = SDL_CreateRGBSurface(SDL_HWSURFACE, Scratch::projectWidth, Scratch::projectHeight, 32, RMASK, GMASK, BMASK, AMASK);

    return true;
}

void Render::penMove(double x1, double y1, double x2, double y2, Sprite *sprite) {
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);

    int penWidth = penSurface->w;
    int penHeight = penSurface->h;

    SDL_Surface *tempSurface = SDL_CreateRGBSurface(SDL_HWSURFACE, penWidth, penHeight, 32, RMASK, GMASK, BMASK, AMASK);
    SDL_FillRect(tempSurface, NULL, SDL_MapRGBA(tempSurface->format, 0, 0, 0, 0));
    SDL_SetAlpha(tempSurface, SDL_SRCALPHA, (100 - sprite->penData.color.transparency) / 100.0f * 255);

    const double scale = (penHeight / static_cast<double>(Scratch::projectHeight));

    const double dx = x2 * scale - x1 * scale;
    const double dy = y2 * scale - y1 * scale;

    const double length = sqrt(dx * dx + dy * dy);
    const double drawWidth = (sprite->penData.size / 2.0f) * scale;

    if (length > 0) {
        const double nx = dx / length;
        const double ny = dy / length;

        int16_t vx[4], vy[4];
        vx[0] = static_cast<int16_t>(x1 * scale + penWidth / 2.0f - ny * drawWidth);
        vy[0] = static_cast<int16_t>(-y1 * scale + penHeight / 2.0f + nx * drawWidth);
        vx[1] = static_cast<int16_t>(x1 * scale + penWidth / 2.0f + ny * drawWidth);
        vy[1] = static_cast<int16_t>(-y1 * scale + penHeight / 2.0f - nx * drawWidth);
        vx[2] = static_cast<int16_t>(x2 * scale + penWidth / 2.0f + ny * drawWidth);
        vy[2] = static_cast<int16_t>(-y2 * scale + penHeight / 2.0f - nx * drawWidth);
        vx[3] = static_cast<int16_t>(x2 * scale + penWidth / 2.0 - ny * drawWidth);
        vy[3] = static_cast<int16_t>(-y2 * scale + penHeight / 2.0f + nx * drawWidth);

        filledPolygonRGBA(tempSurface, vx, vy, 4, rgbColor.r, rgbColor.g, rgbColor.b, 255);
    }

    filledCircleRGBA(tempSurface, x1 * scale + penWidth / 2.0f, -y1 * scale + penHeight / 2.0f, drawWidth, rgbColor.r, rgbColor.g, rgbColor.b, 255);
    filledCircleRGBA(tempSurface, x2 * scale + penWidth / 2.0f, -y2 * scale + penHeight / 2.0f, drawWidth, rgbColor.r, rgbColor.g, rgbColor.b, 255);

    SDL_gfxBlitRGBA(tempSurface, NULL, penSurface, NULL);
    SDL_FreeSurface(tempSurface);
}

void Render::penDot(Sprite *sprite) {
    int penWidth = penSurface->w;
    int penHeight = penSurface->h;

    SDL_Surface *tempSurface = SDL_CreateRGBSurface(SDL_HWSURFACE, penWidth, penHeight, 32, RMASK, GMASK, BMASK, AMASK);
    SDL_FillRect(tempSurface, NULL, SDL_MapRGBA(tempSurface->format, 0, 0, 0, 0));
    SDL_SetAlpha(tempSurface, SDL_SRCALPHA, (100 - sprite->penData.color.transparency) / 100.0f * 255);

    const double scale = (penHeight / static_cast<double>(Scratch::projectHeight));

    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    filledCircleRGBA(tempSurface, sprite->xPosition * scale + penWidth / 2.0f, -sprite->yPosition * scale + penHeight / 2.0f, (sprite->penData.size / 2.0f) * scale, rgbColor.r, rgbColor.g, rgbColor.b, 255);

    SDL_gfxBlitRGBA(tempSurface, NULL, penSurface, NULL);
    SDL_FreeSurface(tempSurface);
}

void Render::penStamp(Sprite *sprite) {
    const auto &imgFind = images.find(sprite->costumes[sprite->currentCostume].id);
    if (imgFind == images.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return;
    }
    imgFind->second->freeTimer = imgFind->second->maxFreeTime;

    // IDK if these are needed
    sprite->rotationCenterX = sprite->costumes[sprite->currentCostume].rotationCenterX;
    sprite->rotationCenterY = sprite->costumes[sprite->currentCostume].rotationCenterY;

    // TODO: remove duplicate code (maybe make a Render::drawSprite function.)
    SDL_Image *image = imgFind->second;
    image->freeTimer = image->maxFreeTime;
    sprite->rotationCenterX = sprite->costumes[sprite->currentCostume].rotationCenterX;
    sprite->rotationCenterY = sprite->costumes[sprite->currentCostume].rotationCenterY;
    sprite->spriteWidth = image->textureRect.w >> 1;
    sprite->spriteHeight = image->textureRect.h >> 1;
    bool flip = false;
    const bool isSVG = sprite->costumes[sprite->currentCostume].isSVG;
    Render::calculateRenderPosition(sprite, isSVG);
    image->renderRect.x = sprite->renderInfo.renderX;
    image->renderRect.y = sprite->renderInfo.renderY;

    if (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0) {
        flip = true;
        image->renderRect.x += (sprite->spriteWidth * (isSVG ? 2 : 1)) * 1.125; // Don't ask why I'm multiplying by 1.125 here, I also have no idea, but it makes it work so...
    }

    // Pen mapping stuff
    const auto &cords = screenToScratchCoords(image->renderRect.x, image->renderRect.y, windowWidth, windowHeight);
    image->renderRect.x = cords.first + Scratch::projectWidth / 2;
    image->renderRect.y = -cords.second + Scratch::projectHeight / 2;

    if (Scratch::hqpen) {
        image->setScale(sprite->renderInfo.renderScaleY);

        const double scale = (penSurface->h / static_cast<double>(Scratch::projectHeight));

        image->renderRect.x *= scale;
        image->renderRect.y *= scale;
    } else {
        image->setScale(sprite->size / (isSVG ? 100.0f : 200.0f));
    }

    // set ghost effect
    float ghost = std::clamp(sprite->ghostEffect, 0.0f, 100.0f);
    Uint8 alpha = static_cast<Uint8>(255 * (1.0f - ghost / 100.0f));
    SDL_SetAlpha(image->spriteTexture, SDL_SRCALPHA, alpha);

    SDL_Surface *finalSurface = rotozoomSurfaceXY(image->spriteTexture, -Math::radiansToDegrees(sprite->renderInfo.renderRotation), sprite->renderInfo.renderScaleX, sprite->renderInfo.renderScaleY, SMOOTHING_OFF);

    if (flip) {
        SDL_Surface *flipped = zoomSurface(finalSurface, -1, 1, 0);
        SDL_FreeSurface(finalSurface);
        finalSurface = flipped;
    }

    SDL_Rect dest;
    dest.x = sprite->renderInfo.renderX + (image->spriteTexture->w * sprite->renderInfo.renderScaleX / 2) - (finalSurface->w / 2);
    dest.y = sprite->renderInfo.renderY + (image->spriteTexture->h * sprite->renderInfo.renderScaleY / 2) - (finalSurface->h / 2);

    SDL_gfxBlitRGBA(finalSurface, NULL, penSurface, &dest);
    SDL_FreeSurface(finalSurface);
}

void Render::penClear() {
    SDL_FillRect(penSurface, NULL, SDL_MapRGBA(penSurface->format, 0, 0, 0, 0));
}

void Render::beginFrame(int screen, int colorR, int colorG, int colorB) {
    if (!hasFrameBegan) {
        SDL_FillRect(window, NULL, SDL_MapRGB(window->format, colorR, colorG, colorB));
        hasFrameBegan = true;
    }
}

void Render::endFrame(bool shouldFlush) {
    SDL_Flip(window);
    SDL_Delay(16);
    if (shouldFlush) Image::FlushImages();
    hasFrameBegan = false;
}

void Render::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    SDL_Rect rect = {static_cast<Sint16>(x - (w / 2)), static_cast<Sint16>(y - (h / 2)), static_cast<Uint16>(w), static_cast<Uint16>(h)};
    SDL_FillRect(window, &rect, SDL_MapRGBA(window->format, colorR, colorG, colorB, colorA));
}

std::pair<float, float> screenToScratchCoords(float screenX, float screenY, int windowWidth, int windowHeight) {
    float screenAspect = static_cast<float>(windowWidth) / windowHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;

    float scratchX, scratchY;

    if (screenAspect > projectAspect) {
        // Vertical black bars
        float scale = static_cast<float>(windowHeight) / Scratch::projectHeight;
        float scaledProjectWidth = Scratch::projectWidth * scale;
        float barWidth = (windowWidth - scaledProjectWidth) / 2.0f;

        // Remove bar offset and scale to project space
        float adjustedX = screenX - barWidth;
        scratchX = (adjustedX / scaledProjectWidth) * Scratch::projectWidth - (Scratch::projectWidth / 2.0f);
        scratchY = (Scratch::projectHeight / 2.0f) - (screenY / windowHeight) * Scratch::projectHeight;

    } else if (screenAspect < projectAspect) {
        // Horizontal black bars
        float scale = static_cast<float>(windowWidth) / Scratch::projectWidth;
        float scaledProjectHeight = Scratch::projectHeight * scale;
        float barHeight = (windowHeight - scaledProjectHeight) / 2.0f;

        // Remove bar offset and scale to project space
        float adjustedY = screenY - barHeight;
        scratchX = (screenX / windowWidth) * Scratch::projectWidth - (Scratch::projectWidth / 2.0f);
        scratchY = (Scratch::projectHeight / 2.0f) - (adjustedY / scaledProjectHeight) * Scratch::projectHeight;

    } else {
        // no black bars..
        scratchX = (screenX / windowWidth) * Scratch::projectWidth - (Scratch::projectWidth / 2.0f);
        scratchY = (Scratch::projectHeight / 2.0f) - (screenY / windowHeight) * Scratch::projectHeight;
    }

    return std::make_pair(scratchX, scratchY);
}

void drawBlackBars(int screenWidth, int screenHeight) {
    float screenAspect = static_cast<float>(screenWidth) / screenHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;

    if (screenAspect > projectAspect) {
        // Vertical bars,,,
        float scale = static_cast<float>(screenHeight) / Scratch::projectHeight;
        float scaledProjectWidth = Scratch::projectWidth * scale;
        float barWidth = (screenWidth - scaledProjectWidth) / 2.0f;

        SDL_Rect leftBar = {0, 0, static_cast<Uint16>(std::ceil(barWidth)), static_cast<Uint16>(screenHeight)};
        SDL_Rect rightBar = {static_cast<Sint16>(std::floor(screenWidth - barWidth)), 0, static_cast<Uint16>(std::ceil(barWidth)), static_cast<Uint16>(screenHeight)};

        SDL_FillRect(window, &leftBar, SDL_MapRGB(window->format, 0, 0, 0));
        SDL_FillRect(window, &rightBar, SDL_MapRGB(window->format, 0, 0, 0));
    } else if (screenAspect < projectAspect) {
        // Horizontal bars,,,
        float scale = static_cast<float>(screenWidth) / Scratch::projectWidth;
        float scaledProjectHeight = Scratch::projectHeight * scale;
        float barHeight = (windowHeight - scaledProjectHeight) / 2.0f;

        SDL_Rect topBar = {0, 0, static_cast<Uint16>(screenWidth), static_cast<Uint16>(std::ceil(barHeight))};
        SDL_Rect bottomBar = {0, static_cast<Sint16>(std::floor(screenHeight - barHeight)), static_cast<Uint16>(screenWidth), static_cast<Uint16>(std::ceil(barHeight))};

        SDL_FillRect(window, &topBar, SDL_MapRGB(window->format, 0, 0, 0));
        SDL_FillRect(window, &bottomBar, SDL_MapRGB(window->format, 0, 0, 0));
    }
}

void Render::renderSprites() {
    SDL_FillRect(window, NULL, SDL_MapRGB(window->format, 255, 255, 255));

    for (auto it = sprites.rbegin(); it != sprites.rend(); ++it) {
        Sprite *currentSprite = *it;
        if (!currentSprite->visible) continue;

        auto imgFind = images.find(currentSprite->costumes[currentSprite->currentCostume].id);
        if (imgFind != images.end()) {
            SDL_Image *image = imgFind->second;
            image->freeTimer = image->maxFreeTime;
            currentSprite->rotationCenterX = currentSprite->costumes[currentSprite->currentCostume].rotationCenterX;
            currentSprite->rotationCenterY = currentSprite->costumes[currentSprite->currentCostume].rotationCenterY;
            currentSprite->spriteWidth = image->textureRect.w >> 1;
            currentSprite->spriteHeight = image->textureRect.h >> 1;
            bool flip = false;
            const bool isSVG = currentSprite->costumes[currentSprite->currentCostume].isSVG;
            calculateRenderPosition(currentSprite, isSVG);
            image->renderRect.x = currentSprite->renderInfo.renderX;
            image->renderRect.y = currentSprite->renderInfo.renderY;

            image->setScale(currentSprite->renderInfo.renderScaleY);
            if (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT && currentSprite->rotation < 0) {
                flip = true;
                image->renderRect.x += (currentSprite->spriteWidth * (isSVG ? 2 : 1)) * 1.125; // Don't ask why I'm multiplying by 1.125 here, I also have no idea, but it makes it work so...
            }

            // set ghost effect
            float ghost = std::clamp(currentSprite->ghostEffect, 0.0f, 100.0f);
            Uint8 alpha = static_cast<Uint8>(255 * (1.0f - ghost / 100.0f));
            SDL_SetAlpha(image->spriteTexture, SDL_SRCALPHA, alpha);

            // TODO: implement brightness effect

            SDL_Surface *finalSurface = rotozoomSurfaceXY(image->spriteTexture, -Math::radiansToDegrees(currentSprite->renderInfo.renderRotation), currentSprite->renderInfo.renderScaleX, currentSprite->renderInfo.renderScaleY, SMOOTHING_OFF);

            if (flip) {
                SDL_Surface *flipped = zoomSurface(finalSurface, -1, 1, 0);
                SDL_FreeSurface(finalSurface);
                finalSurface = flipped;
            }

            SDL_Rect dest;
            dest.x = currentSprite->renderInfo.renderX + (image->spriteTexture->w * currentSprite->renderInfo.renderScaleX / 2) - (finalSurface->w / 2);
            dest.y = currentSprite->renderInfo.renderY + (image->spriteTexture->h * currentSprite->renderInfo.renderScaleY / 2) - (finalSurface->h / 2);

            SDL_BlitSurface(finalSurface, NULL, window, &dest);
            SDL_FreeSurface(finalSurface);
        }

        // Draw collision points (for debugging)
        // std::vector<std::pair<double, double>> collisionPoints = getCollisionPoints(currentSprite);
        // SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black points

        // for (const auto &point : collisionPoints) {
        //     double screenX = (point.first * renderScale) + (windowWidth / 2);
        //     double screenY = (point.second * -renderScale) + (windowHeight / 2);

        //     SDL_Rect debugPointRect;
        //     debugPointRect.x = static_cast<int>(screenX - renderScale); // center it a bit
        //     debugPointRect.y = static_cast<int>(screenY - renderScale);
        //     debugPointRect.w = static_cast<int>(2 * renderScale);
        //     debugPointRect.h = static_cast<int>(2 * renderScale);

        //     SDL_RenderFillRect(renderer, &debugPointRect);
        // }

        if (currentSprite->isStage) renderPenLayer();
    }

    drawBlackBars(windowWidth, windowHeight);
    renderVisibleVariables();

    SDL_Flip(window);
    Image::FlushImages();
    SoundPlayer::flushAudio();
}

std::unordered_map<std::string, TextObject *> Render::monitorTexts;

void Render::renderPenLayer() {
    if (penSurface == nullptr) return;

    SDL_Rect renderRect = {0, 0, 0, 0};

    if (static_cast<float>(windowWidth) / windowHeight > static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight) {
        renderRect.x = std::ceil((windowWidth - Scratch::projectWidth * (static_cast<float>(windowHeight) / Scratch::projectHeight)) / 2.0f);
        renderRect.w = windowWidth - renderRect.x * 2;
        renderRect.h = windowHeight;
    } else {
        renderRect.y = std::ceil((windowHeight - Scratch::projectHeight * (static_cast<float>(windowWidth) / Scratch::projectWidth)) / 2.0f);
        renderRect.h = windowHeight - renderRect.y * 2;
        renderRect.w = windowWidth;
    }

    SDL_Surface *zoomedSurface = zoomSurface(penSurface, renderRect.w / penSurface->w, renderRect.h / penSurface->h, SMOOTHING_OFF);
    SDL_BlitSurface(zoomedSurface, NULL, window, NULL);
    SDL_FreeSurface(zoomedSurface);
}

bool Render::appShouldRun() {
    if (toExit) return false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            toExit = true;
            return false;
        case SDL_VIDEORESIZE:
            windowWidth = event.resize.w;
            windowHeight = event.resize.h;
            window = SDL_SetVideoMode(windowWidth, windowHeight, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_RESIZABLE);
            setRenderScale();

            if (!Scratch::hqpen) break;

            int width, height;
            if (Scratch::projectWidth / static_cast<double>(windowWidth) < Scratch::projectHeight / static_cast<double>(windowHeight)) {
                width = Scratch::projectWidth * (windowHeight / static_cast<double>(Scratch::projectHeight));
                height = windowHeight;
            } else {
                width = windowWidth;
                height = Scratch::projectHeight * (windowWidth / static_cast<double>(Scratch::projectWidth));
            }

            SDL_Surface *zoomedSurface = zoomSurface(penSurface, width / penSurface->w, height / penSurface->h, SMOOTHING_OFF);
            SDL_FreeSurface(penSurface);
            penSurface = zoomedSurface;

            break;
        }
    }
    return true;
}
