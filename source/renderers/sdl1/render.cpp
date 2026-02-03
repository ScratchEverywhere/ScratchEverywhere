#include "render.hpp"
#include "image.hpp"
#include "speech_manager_sdl1.hpp"
#include <SDL/SDL.h>
#include <SDL/SDL_gfxBlitFunc.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_rotozoom.h>
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
#include <sprite.hpp>
#include <string>
#include <text.hpp>
#include <unordered_map>
#include <unzip.hpp>
#include <vector>
#include <window.hpp>
#include <windowing/sdl1/window.hpp>

#ifdef __MINGW32__
#define filledCircleRGBA GFX_filledCircleRGBA
#define filledPolygonRGBA GFX_filledPolygonRGBA
#endif

#ifdef __XBOX360__
#include <console/console.h>
#include <diskio/ata.h>
#include <libfat/fat.h>
#include <usb/usbmain.h>
#include <xenon_soc/xenon_power.h>
#include <xenon_sound/sound.h>
#include <xenos/xe.h>
#include <xenos/xenos.h>
#endif

Window *globalWindow = nullptr;
SDL_Surface *penSurface = nullptr;

SpeechManagerSDL1 *speechManager = nullptr;

Render::RenderModes Render::renderMode = Render::TOP_SCREEN_ONLY;
bool Render::hasFrameBegan;
std::vector<Monitor> Render::visibleVariables;
std::chrono::system_clock::time_point Render::startTime = std::chrono::system_clock::now();
std::chrono::system_clock::time_point Render::endTime = std::chrono::system_clock::now();
bool Render::debugMode = false;
float Render::renderScale = 1.0f;

bool Render::Init() {
#ifdef __XBOX360__
    xenon_make_it_faster(XENON_SPEED_FULL);
    xenos_init(VIDEO_MODE_AUTO);
    xenon_sound_init();
    console_init();
	usb_init();
	usb_do_poll();
    xenon_ata_init();
    xenon_atapi_init();
    fatInitDefault();

    int windowWidth = 640;
    int windowHeight = 480;
#else
    int windowWidth = 540;
    int windowHeight = 405;
#endif
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    TTF_Init();

    globalWindow = new WindowSDL1();
    if (!globalWindow->init(windowWidth, windowHeight, "Scratch Everywhere!")) {
        delete globalWindow;
        globalWindow = nullptr;
        return false;
    }

    speechManager = new SpeechManagerSDL1(static_cast<SDL_Surface *>(globalWindow->getHandle()));

    debugMode = true;

    return true;
}
void Render::deInit() {
    if (speechManager) {
        delete speechManager;
        speechManager = nullptr;
    }

    SDL_FreeSurface(penSurface);

    Image::cleanupImages();
    SoundPlayer::cleanupAudio();
    TextObject::cleanupText();

    if (globalWindow) {
        globalWindow->cleanup();
        delete globalWindow;
        globalWindow = nullptr;
    }

    SoundPlayer::deinit();
    IMG_Quit();
    SDL_Quit();
}

void *Render::getRenderer() {
    if (globalWindow) return globalWindow->getHandle();
    return nullptr;
}

SpeechManager *Render::getSpeechManager() {
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
    if (penSurface != nullptr) return true;

    if (Scratch::hqpen) {
        if (Scratch::projectWidth / static_cast<double>(getWidth()) < Scratch::projectHeight / static_cast<double>(getHeight()))
            penSurface = SDL_CreateRGBSurface(SDL_HWSURFACE, Scratch::projectWidth * (getHeight() / static_cast<double>(Scratch::projectHeight)), getHeight(), 32, RMASK, GMASK, BMASK, AMASK);
        else
            penSurface = SDL_CreateRGBSurface(SDL_HWSURFACE, getWidth(), Scratch::projectHeight * (getWidth() / static_cast<double>(Scratch::projectWidth)), 32, RMASK, GMASK, BMASK, AMASK);
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
    }

    // Pen mapping stuff
    const auto &cords = Scratch::screenToScratchCoords(image->renderRect.x, image->renderRect.y, getWidth(), getHeight());
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
    SDL_Surface *window = static_cast<SDL_Surface *>(getRenderer());
    if (!hasFrameBegan) {
        SDL_FillRect(window, NULL, SDL_MapRGB(window->format, colorR, colorG, colorB));
        hasFrameBegan = true;
    }
}

void Render::endFrame(bool shouldFlush) {
    SDL_Flip(static_cast<SDL_Surface *>(getRenderer()));
    SDL_Delay(16);
    if (shouldFlush) Image::FlushImages();
    hasFrameBegan = false;
}

void Render::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    SDL_Surface *window = static_cast<SDL_Surface *>(getRenderer());
    SDL_Rect rect = {static_cast<Sint16>(x - (w / 2)), static_cast<Sint16>(y - (h / 2)), static_cast<Uint16>(w), static_cast<Uint16>(h)};
    SDL_FillRect(window, &rect, SDL_MapRGBA(window->format, colorR, colorG, colorB, colorA));
}

void drawBlackBars(int screenWidth, int screenHeight) {
    SDL_Surface *window = static_cast<SDL_Surface *>(Render::getRenderer());
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
        float barHeight = (Render::getHeight() - scaledProjectHeight) / 2.0f;

        SDL_Rect topBar = {0, 0, static_cast<Uint16>(screenWidth), static_cast<Uint16>(std::ceil(barHeight))};
        SDL_Rect bottomBar = {0, static_cast<Sint16>(std::floor(screenHeight - barHeight)), static_cast<Uint16>(screenWidth), static_cast<Uint16>(std::ceil(barHeight))};

        SDL_FillRect(window, &topBar, SDL_MapRGB(window->format, 0, 0, 0));
        SDL_FillRect(window, &bottomBar, SDL_MapRGB(window->format, 0, 0, 0));
    }
}

void Render::renderSprites() {
    SDL_Surface *window = static_cast<SDL_Surface *>(getRenderer());
    SDL_FillRect(window, NULL, SDL_MapRGB(window->format, 255, 255, 255));

    for (auto it = Scratch::sprites.rbegin(); it != Scratch::sprites.rend(); ++it) {
        Sprite *currentSprite = *it;

        auto imgFind = images.find(currentSprite->costumes[currentSprite->currentCostume].id);
        if (imgFind != images.end()) {
            SDL_Image *image = imgFind->second;

            currentSprite->rotationCenterX = currentSprite->costumes[currentSprite->currentCostume].rotationCenterX;
            currentSprite->rotationCenterY = currentSprite->costumes[currentSprite->currentCostume].rotationCenterY;
            currentSprite->spriteWidth = image->textureRect.w >> 1;
            currentSprite->spriteHeight = image->textureRect.h >> 1;

            if (!currentSprite->visible) continue;

            image->freeTimer = image->maxFreeTime;

            bool flip = false;
            const bool isSVG = currentSprite->costumes[currentSprite->currentCostume].isSVG;
            calculateRenderPosition(currentSprite, isSVG);
            image->renderRect.x = currentSprite->renderInfo.renderX;
            image->renderRect.y = currentSprite->renderInfo.renderY;

            image->setScale(currentSprite->renderInfo.renderScaleY);
            if (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT && currentSprite->rotation < 0) {
                flip = true;
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
        // std::vector<std::pair<double, double>> collisionPoints = Scratch::getCollisionPoints(currentSprite);
        // SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black points

        // for (const auto &point : collisionPoints) {
        //     double screenX = (point.first * renderScale) + (getWidth() / 2);
        //     double screenY = (point.second * -renderScale) + (getHeight() / 2);

        //     SDL_Rect debugPointRect;
        //     debugPointRect.x = static_cast<int>(screenX - renderScale); // center it a bit
        //     debugPointRect.y = static_cast<int>(screenY - renderScale);
        //     debugPointRect.w = static_cast<int>(2 * renderScale);
        //     debugPointRect.h = static_cast<int>(2 * renderScale);

        //     SDL_RenderFillRect(renderer, &debugPointRect);
        // }

        if (currentSprite->isStage) renderPenLayer();
    }

    if (speechManager) {
        speechManager->render();
    }

    drawBlackBars(getWidth(), getHeight());
    renderVisibleVariables();

    if (globalWindow) globalWindow->swapBuffers();
    Image::FlushImages();
    SoundPlayer::flushAudio();
}

std::unordered_map<std::string, std::pair<std::unique_ptr<TextObject>, std::unique_ptr<TextObject>>> Render::monitorTexts;
std::unordered_map<std::string, Render::ListMonitorRenderObjects> Render::listMonitors;

void Render::renderPenLayer() {
    if (penSurface == nullptr) return;

    SDL_Rect renderRect = {0, 0, 0, 0};
    if (static_cast<float>(getWidth()) / getHeight() > static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight) {
        renderRect.x = std::ceil((getWidth() - Scratch::projectWidth * (static_cast<float>(getHeight()) / Scratch::projectHeight)) / 2.0f);
        renderRect.w = getWidth() - renderRect.x * 2;
        renderRect.h = getHeight();
    } else {
        renderRect.y = std::ceil((getHeight() - Scratch::projectHeight * (static_cast<float>(getWidth()) / Scratch::projectWidth)) / 2.0f);
        renderRect.h = getHeight() - renderRect.y * 2;
        renderRect.w = getWidth();
    }

    SDL_Surface *zoomedSurface = zoomSurface(penSurface, (float)renderRect.w / penSurface->w, (float)renderRect.h / penSurface->h, SMOOTHING_OFF);
    SDL_BlitSurface(zoomedSurface, NULL, static_cast<SDL_Surface *>(getRenderer()), &renderRect);
    SDL_FreeSurface(zoomedSurface);
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
                int width, height;
                if (Scratch::projectWidth / static_cast<double>(currentW) < Scratch::projectHeight / static_cast<double>(currentH)) {
                    width = Scratch::projectWidth * (currentH / static_cast<double>(Scratch::projectHeight));
                    height = currentH;
                } else {
                    width = currentW;
                    height = Scratch::projectHeight * (currentW / static_cast<double>(Scratch::projectWidth));
                }

                SDL_Surface *zoomedSurface = zoomSurface(penSurface, width / penSurface->w, height / penSurface->h, SMOOTHING_OFF);
                SDL_FreeSurface(penSurface);
                penSurface = zoomedSurface;
            }
        }

        return !globalWindow->shouldClose();
    }
    return true;
}
