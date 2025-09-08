#include "../scratch/render.hpp"
#include "../scratch/audio.hpp"
#include "../scratch/unzip.hpp"
#include "image.hpp"
#include "interpret.hpp"
#include "math.hpp"
#include "render.hpp"
#include "sprite.hpp"
#include "text.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_gamecontroller.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_joystick.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_video.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef __WIIU__
#include <coreinit/debug.h>
#include <nn/act.h>
#include <romfs-wiiu.h>
#include <whb/log_udp.h>
#include <whb/sdcard.h>
#endif

#ifdef __SWITCH__
#include <switch.h>

char nickname[0x21];
#endif

#ifdef VITA
#include <psp2/touch.h>
#endif

#ifdef __OGC__
#include <fat.h>
#include <romfs-ogc.h>
#endif

int windowWidth = 540;
int windowHeight = 405;
SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;

Render::RenderModes Render::renderMode = Render::TOP_SCREEN_ONLY;
bool Render::hasFrameBegan;
std::vector<Monitor> Render::visibleVariables;
std::chrono::_V2::system_clock::time_point Render::startTime = std::chrono::high_resolution_clock::now();
std::chrono::_V2::system_clock::time_point Render::endTime = std::chrono::high_resolution_clock::now();
bool Render::debugMode = false;

// TODO: properly export these to input.cpp
SDL_GameController *controller;
bool touchActive = false;
SDL_Point touchPosition;

bool Render::Init() {
#ifdef __WIIU__
    WHBLogUdpInit();

    if (romfsInit()) {
        OSFatal("Failed to init romfs.");
        return false;
    }
    if (!WHBMountSdCard()) {
        OSFatal("Failed to mount sd card.");
        return false;
    }
    nn::act::Initialize();

    windowWidth = 854;
    windowHeight = 480;
#elif defined(__SWITCH__)
    AccountUid userID = {0};
    AccountProfile profile;
    AccountProfileBase profilebase;
    memset(&profilebase, 0, sizeof(profilebase));

    Result rc = romfsInit();
    if (R_FAILED(rc)) {
        Log::logError("Failed to init romfs."); // TODO: Include error code
        goto postAccount;
    }

    rc = accountInitialize(AccountServiceType_Application);
    if (R_FAILED(rc)) {
        Log::logError("accountInitialize failed.");
        goto postAccount;
    }

    rc = accountGetPreselectedUser(&userID);
    if (R_FAILED(rc)) {
        PselUserSelectionSettings settings;
        memset(&settings, 0, sizeof(settings));
        rc = pselShowUserSelector(&userID, &settings);
        if (R_FAILED(rc)) {
            Log::logError("pselShowUserSelector failed.");
            goto postAccount;
        }
    }

    rc = accountGetProfile(&profile, userID);
    if (R_FAILED(rc)) {
        Log::logError("accountGetProfile failed.");
        goto postAccount;
    }

    rc = accountProfileGet(&profile, NULL, &profilebase);
    if (R_FAILED(rc)) {
        Log::logError("accountProfileGet failed.");
        goto postAccount;
    }

    memset(nickname, 0, sizeof(nickname));
    strncpy(nickname, profilebase.nickname, sizeof(nickname) - 1);

    accountProfileClose(&profile);
    accountExit();
postAccount:
#elif defined(__OGC__)
    SYS_STDIO_Report(true);

    fatInitDefault();
    windowWidth = 640;
    windowHeight = 480;
    if (romfsInit()) {
        Log::logError("Failed to init romfs.");
        return false;
    }
#elif defined(VITA)
    SDL_setenv("VITA_DISABLE_TOUCH_BACK", "1", 1);

    windowWidth = 960;
    windowHeight = 544;
#endif
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
#ifdef ENABLE_AUDIO
    // Initialize SDL_mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        Log::logWarning(std::string("SDL_Mixer could not initialize! ") + Mix_GetError());
        return false;
    }
    int flags = MIX_INIT_MP3 | MIX_INIT_OGG;
    if (Mix_Init(flags) != flags) {
        Log::logWarning(std::string("SDL_Mixer could not initialize MP3/OGG Support! ") + Mix_GetError());
    }
#endif
    TTF_Init();
    window = SDL_CreateWindow("Scratch Runtime", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (SDL_NumJoysticks() > 0) controller = SDL_GameControllerOpen(0);

    debugMode = true;
    return true;
}
void Render::deInit() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SoundPlayer::deinit();
    IMG_Quit();
    SDL_Quit();

#if defined(__WIIU__) || defined(__SWITCH__)
    romfsExit();
#endif
#ifdef __WIIU__
    WHBUnmountSdCard();
    nn::act::Finalize();
#endif
#ifdef __OGC__
    romfsExit();
#endif
}

void *Render::getRenderer() {
    return static_cast<void *>(renderer);
}

int Render::getWidth() {
    SDL_GetWindowSizeInPixels(window, &windowWidth, &windowHeight);
    return windowWidth;
}
int Render::getHeight() {
    SDL_GetWindowSizeInPixels(window, &windowWidth, &windowHeight);
    return windowHeight;
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

void Render::drawBox(int w, int h, int x, int y, int colorR, int colorG, int colorB, int colorA) {
    SDL_SetRenderDrawColor(renderer, colorR, colorG, colorB, colorA);
    SDL_Rect rect = {x - (w / 2), y - (h / 2), w, h};
    SDL_RenderFillRect(renderer, &rect);
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

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_Rect leftBar = {0, 0, static_cast<int>(std::ceil(barWidth)), screenHeight};
        SDL_Rect rightBar = {static_cast<int>(std::floor(screenWidth - barWidth)), 0, static_cast<int>(std::ceil(barWidth)), screenHeight};

        SDL_RenderFillRect(renderer, &leftBar);
        SDL_RenderFillRect(renderer, &rightBar);
    } else if (screenAspect < projectAspect) {
        // Horizontal bars,,,
        float scale = static_cast<float>(screenWidth) / Scratch::projectWidth;
        float scaledProjectHeight = Scratch::projectHeight * scale;
        float barHeight = (screenHeight - scaledProjectHeight) / 2.0f;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_Rect topBar = {0, 0, screenWidth, static_cast<int>(std::ceil(barHeight))};
        SDL_Rect bottomBar = {0, static_cast<int>(std::floor(screenHeight - barHeight)), screenWidth, static_cast<int>(std::ceil(barHeight))};

        SDL_RenderFillRect(renderer, &topBar);
        SDL_RenderFillRect(renderer, &bottomBar);
    }
}

void Render::renderSprites() {
    SDL_GetWindowSizeInPixels(window, &windowWidth, &windowHeight);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    double scaleX = static_cast<double>(windowWidth) / Scratch::projectWidth;
    double scaleY = static_cast<double>(windowHeight) / Scratch::projectHeight;
    double scale;
    scale = std::min(scaleX, scaleY);

    // Sort sprites by layer with stage always being first
    std::vector<Sprite *> spritesByLayer = sprites;
    std::sort(spritesByLayer.begin(), spritesByLayer.end(),
              [](const Sprite *a, const Sprite *b) {
                  // Stage sprite always comes first
                  if (a->isStage && !b->isStage) return true;
                  if (!a->isStage && b->isStage) return false;
                  // Otherwise sort by layer
                  return a->layer < b->layer;
              });

    for (Sprite *currentSprite : spritesByLayer) {
        if (!currentSprite->visible) continue;

        bool legacyDrawing = false;
        auto imgFind = images.find(currentSprite->costumes[currentSprite->currentCostume].id);
        if (imgFind == images.end()) {
            legacyDrawing = true;
        } else {
            currentSprite->rotationCenterX = currentSprite->costumes[currentSprite->currentCostume].rotationCenterX;
            currentSprite->rotationCenterY = currentSprite->costumes[currentSprite->currentCostume].rotationCenterY;
        }
        if (!legacyDrawing) {
            SDL_Image *image = imgFind->second;
            image->freeTimer = image->maxFreeTime;
            SDL_RendererFlip flip = SDL_FLIP_NONE;
            image->setScale((currentSprite->size * 0.01) * scale / 2.0f);
            currentSprite->spriteWidth = image->textureRect.w / 2;
            currentSprite->spriteHeight = image->textureRect.h / 2;
            if (image->isSVG) {
                image->setScale(image->scale * 2);
            }
            const double rotation = Math::degreesToRadians(currentSprite->rotation - 90.0f);
            double renderRotation = rotation;
            if (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT) {
                if (std::cos(rotation) < 0) {
                    flip = SDL_FLIP_HORIZONTAL;
                }
                renderRotation = 0;
            }
            if (currentSprite->rotationStyle == currentSprite->NONE) {
                renderRotation = 0;
            }
            double rotationCenterX = ((((currentSprite->rotationCenterX - currentSprite->spriteWidth)) / 2) * scale);
            double rotationCenterY = ((((currentSprite->rotationCenterY - currentSprite->spriteHeight)) / 2) * scale);
            const double offsetX = rotationCenterX * (currentSprite->size * 0.01);
            const double offsetY = rotationCenterY * (currentSprite->size * 0.01);
            image->renderRect.x = ((currentSprite->xPosition * scale) + (windowWidth / 2) - (image->renderRect.w / 2)) - offsetX * std::cos(rotation) + offsetY * std::sin(renderRotation);
            image->renderRect.y = ((currentSprite->yPosition * -scale) + (windowHeight / 2) - (image->renderRect.h / 2)) - offsetX * std::sin(rotation) - offsetY * std::cos(renderRotation);
            SDL_Point center = {image->renderRect.w / 2, image->renderRect.h / 2};

            // set ghost effect
            float ghost = std::clamp(currentSprite->ghostEffect, 0.0f, 100.0f);
            Uint8 alpha = static_cast<Uint8>(255 * (1.0f - ghost / 100.0f));
            SDL_SetTextureAlphaMod(image->spriteTexture, alpha);

            // set brightness effect
            if (currentSprite->brightnessEffect != 0) {
                float brightness = currentSprite->brightnessEffect * 0.01f;

                // TODO: find a better way to do this because i hate this
                if (brightness > 0.0f) {
                    // render the normal image first
                    SDL_RenderCopyEx(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                                     Math::radiansToDegrees(renderRotation), &center, flip);

                    // render another, blended image on top
                    SDL_SetTextureBlendMode(image->spriteTexture, SDL_BLENDMODE_ADD);
                    SDL_SetTextureAlphaMod(image->spriteTexture, (Uint8)(brightness * 255 * (alpha / 255.0f)));
                    SDL_RenderCopyEx(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                                     Math::radiansToDegrees(renderRotation), &center, flip);

                    // reset for next frame
                    SDL_SetTextureBlendMode(image->spriteTexture, SDL_BLENDMODE_BLEND);
                } else {
                    // darkening is quite shrimple really
                    Uint8 col = static_cast<Uint8>(255 * (1.0f + brightness));
                    SDL_SetTextureColorMod(image->spriteTexture, col, col, col);

                    SDL_RenderCopyEx(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                                     Math::radiansToDegrees(renderRotation), &center, flip);
                    // reset for next frame
                    SDL_SetTextureColorMod(image->spriteTexture, 255, 255, 255);
                }
            } else {
                // if no brightness just render normal image
                SDL_SetTextureColorMod(image->spriteTexture, 255, 255, 255);
                SDL_RenderCopyEx(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                                 Math::radiansToDegrees(renderRotation), &center, flip);
            }
        } else {
            currentSprite->spriteWidth = 64;
            currentSprite->spriteHeight = 64;
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_Rect rect;
            rect.x = (currentSprite->xPosition * scale) + (windowWidth / 2);
            rect.y = (currentSprite->yPosition * -1 * scale) + (windowHeight * 0.5);
            rect.w = 16;
            rect.h = 16;
            SDL_RenderDrawRect(renderer, &rect);
        }

        // Draw collision points (for debugging)
        // std::vector<std::pair<double, double>> collisionPoints = getCollisionPoints(currentSprite);
        // SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black points

        // for (const auto& point : collisionPoints) {
        //     double screenX = (point.first * scale) + (windowWidth / 2);
        //     double screenY = (point.second * -scale) + (windowHeight / 2);

        //     SDL_Rect debugPointRect;
        //     debugPointRect.x = static_cast<int>(screenX - scale); // center it a bit
        //     debugPointRect.y = static_cast<int>(screenY - scale);
        //     debugPointRect.w = static_cast<int>(2 * scale);
        //     debugPointRect.h = static_cast<int>(2 * scale);

        //     SDL_RenderFillRect(renderer, &debugPointRect);
        // }
    }

    drawBlackBars(windowWidth, windowHeight);
    renderVisibleVariables();

    SDL_RenderPresent(renderer);
    Image::FlushImages();
}

std::unordered_map<std::string, TextObject *> Render::monitorTexts;

void Render::renderVisibleVariables() {
    // get screen scale
    double scaleX = static_cast<double>(windowWidth) / Scratch::projectWidth;
    double scaleY = static_cast<double>(windowHeight) / Scratch::projectHeight;
    double scale = std::min(scaleX, scaleY);

    // calculate black bar offset
    float screenAspect = static_cast<float>(windowWidth) / windowHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;
    float barOffsetX = 0.0f;
    float barOffsetY = 0.0f;
    if (screenAspect > projectAspect) {
        float scaledProjectWidth = Scratch::projectWidth * scale;
        barOffsetX = (windowWidth - scaledProjectWidth) / 2.0f;
    } else if (screenAspect < projectAspect) {
        float scaledProjectHeight = Scratch::projectHeight * scale;
        barOffsetY = (windowHeight - scaledProjectHeight) / 2.0f;
    }

    for (auto &var : visibleVariables) {
        if (var.visible) {
            std::string renderText = BlockExecutor::getMonitorValue(var).asString();
            if (monitorTexts.find(var.id) == monitorTexts.end()) {
                monitorTexts[var.id] = createTextObject(renderText, var.x, var.y);
            } else {
                monitorTexts[var.id]->setText(renderText);
            }
            monitorTexts[var.id]->setColor(0x000000FF);

            if (var.mode != "large") {
                monitorTexts[var.id]->setCenterAligned(false);
                monitorTexts[var.id]->setScale(1.0f * (scale / 2.0f));
            } else {
                monitorTexts[var.id]->setCenterAligned(true);
                monitorTexts[var.id]->setScale(1.25f * (scale / 2.0f));
            }
            monitorTexts[var.id]->render(var.x * scale + barOffsetX, var.y * scale + barOffsetY);
        } else {
            if (monitorTexts.find(var.id) != monitorTexts.end()) {
                delete monitorTexts[var.id];
                monitorTexts.erase(var.id);
            }
        }
    }
}

bool Render::appShouldRun() {
    if (toExit) return false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            toExit = true;
            return false;
        case SDL_CONTROLLERDEVICEADDED:
            controller = SDL_GameControllerOpen(0);
            break;
        case SDL_FINGERDOWN:
            touchActive = true;
            touchPosition = {
                static_cast<int>(event.tfinger.x * windowWidth),
                static_cast<int>(event.tfinger.y * windowHeight)};
            break;
        case SDL_FINGERMOTION:
            touchPosition = {
                static_cast<int>(event.tfinger.x * windowWidth),
                static_cast<int>(event.tfinger.y * windowHeight)};
            break;
        case SDL_FINGERUP:
            touchActive = false;
            break;
        }
    }
    return true;
}
