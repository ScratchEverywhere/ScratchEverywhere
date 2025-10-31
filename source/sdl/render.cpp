#include "../scratch/render.hpp"
#include "../scratch/image.hpp"
#include "audio.hpp"
#include "blocks/pen.hpp"
#include "image.hpp"
#include "interpret.hpp"
#include "math.hpp"
#include "render.hpp"
#include "sprite.hpp"
#include "text.hpp"
#include "unzip.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
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
#include <psp2/io/fcntl.h>
#include <psp2/net/http.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>
#include <psp2/touch.h>
#endif

#ifdef __OGC__
#include <fat.h>
#include <ogc/system.h>
#include <romfs-ogc.h>
#endif

#ifdef __PS4__
#include <orbis/Net.h>
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h>

inline void SDL_GetWindowSizeInPixels(SDL_Window *window, int *w, int *h) {
    // On PS4 there is no DPI scaling, so this is fine
    SDL_GetWindowSize(window, w, h);
}
#endif

#ifdef GAMECUBE
#include <ogc/consol.h>
#include <ogc/exi.h>
#endif

#ifdef __MINGW32__
#define filledCircleRGBA GFX_filledCircleRGBA
#define filledPolygonRGBA GFX_filledPolygonRGBA
#endif

int windowWidth = 540;
int windowHeight = 405;
SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;

Render::RenderModes Render::renderMode = Render::TOP_SCREEN_ONLY;
bool Render::hasFrameBegan;
std::vector<Monitor> Render::visibleVariables;
std::chrono::system_clock::time_point Render::startTime = std::chrono::system_clock::now();
std::chrono::system_clock::time_point Render::endTime = std::chrono::system_clock::now();
bool Render::debugMode = false;
float Render::renderScale = 1.0f;

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

    windowWidth = 1280;
    windowHeight = 720;

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
#ifdef GAMECUBE
    if ((SYS_GetConsoleType() & SYS_CONSOLE_MASK) == SYS_CONSOLE_DEVELOPMENT) {
        CON_EnableBarnacle(EXI_CHANNEL_0, EXI_DEVICE_1);
    }
    CON_EnableGecko(EXI_CHANNEL_1, true);
#else
    SYS_STDIO_Report(true);
#endif

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

    Log::log("[Vita] Loading module SCE_SYSMODULE_NET");
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);

    Log::log("[Vita] Running sceNetInit");
    SceNetInitParam netInitParam;
    int size = 1 * 1024 * 1024; // net buffer size ([size in MB]*1024*1024)
    netInitParam.memory = malloc(size);
    netInitParam.size = size;
    netInitParam.flags = 0;
    sceNetInit(&netInitParam);

    Log::log("[Vita] Running sceNetCtlInit");
    sceNetCtlInit();
#elif defined(__PS4__)
    int rc = sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL);
    if (rc != ORBIS_OK) {
        Log::logError("Failed to init freetype.");
        return false;
    }

    windowWidth = 1280;
    windowHeight = 720;
#endif
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    TTF_Init();
    window = SDL_CreateWindow("Scratch Everywhere!", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (SDL_NumJoysticks() > 0) controller = SDL_GameControllerOpen(0);

    debugMode = true;
    return true;
}
void Render::deInit() {
    SDL_DestroyTexture(penTexture);

    Image::cleanupImages();
    SoundPlayer::cleanupAudio();
    TextObject::cleanupText();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SoundPlayer::deinit();
    IMG_Quit();
    SDL_Quit();

#if defined(__WIIU__) || defined(__SWITCH__) || defined(__OGC__)
    romfsExit();
#endif
#ifdef __WIIU__
    WHBUnmountSdCard();
    nn::act::Finalize();
#endif
}

void *Render::getRenderer() {
    return static_cast<void *>(renderer);
}

int Render::getWidth() {
    return windowWidth;
}
int Render::getHeight() {
    return windowHeight;
}

bool Render::initPen() {
    if (penTexture != nullptr) return true;

    if (Scratch::hqpen) {
        if (Scratch::projectWidth / static_cast<double>(windowWidth) < Scratch::projectHeight / static_cast<double>(windowHeight))
            penTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, Scratch::projectWidth * (windowHeight / static_cast<double>(Scratch::projectHeight)), windowHeight);
        else
            penTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, windowWidth, Scratch::projectHeight * (windowWidth / static_cast<double>(Scratch::projectWidth)));
    } else penTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, Scratch::projectWidth, Scratch::projectHeight);

    // Clear the texture
    SDL_SetTextureBlendMode(penTexture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, penTexture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, nullptr);

    return true;
}

void Render::penMove(double x1, double y1, double x2, double y2, Sprite *sprite) {
    const ColorRGB rgbColor = HSB2RGB(sprite->penData.color);

#if defined(__PC__) || defined(__WIIU__) // Only these platforms seem to support custom blend modes.
    SDL_BlendMode blendMode = SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        SDL_BLENDOPERATION_ADD,

        SDL_BLENDFACTOR_ONE,
        SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        SDL_BLENDOPERATION_ADD);
#else
    SDL_BlendMode blendMode = SDL_BLENDMODE_BLEND;
#endif

    int penWidth;
    int penHeight;
    SDL_QueryTexture(penTexture, NULL, NULL, &penWidth, &penHeight);

    SDL_Texture *tempTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, penWidth, penHeight);
    SDL_SetTextureBlendMode(tempTexture, blendMode);
    SDL_SetTextureAlphaMod(tempTexture, (100 - sprite->penData.transparency) / 100.0f * 255);
    SDL_SetRenderTarget(renderer, tempTexture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

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

        filledPolygonRGBA(renderer, vx, vy, 4, rgbColor.r, rgbColor.g, rgbColor.b, 255);
    }

    filledCircleRGBA(renderer, x1 * scale + penWidth / 2.0f, -y1 * scale + penHeight / 2.0f, drawWidth, rgbColor.r, rgbColor.g, rgbColor.b, 255);
    filledCircleRGBA(renderer, x2 * scale + penWidth / 2.0f, -y2 * scale + penHeight / 2.0f, drawWidth, rgbColor.r, rgbColor.g, rgbColor.b, 255);

    SDL_SetRenderTarget(renderer, penTexture);
    SDL_SetRenderDrawBlendMode(renderer, blendMode);
    SDL_RenderCopy(renderer, tempTexture, NULL, NULL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_DestroyTexture(tempTexture);
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
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

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

        auto imgFind = images.find(currentSprite->costumes[currentSprite->currentCostume].id);
        if (imgFind != images.end()) {
            SDL_Image *image = imgFind->second;
            image->freeTimer = image->maxFreeTime;
            currentSprite->rotationCenterX = currentSprite->costumes[currentSprite->currentCostume].rotationCenterX;
            currentSprite->rotationCenterY = currentSprite->costumes[currentSprite->currentCostume].rotationCenterY;
            currentSprite->spriteWidth = image->textureRect.w >> 1;
            currentSprite->spriteHeight = image->textureRect.h >> 1;
            SDL_RendererFlip flip = SDL_FLIP_NONE;
            const bool isSVG = currentSprite->costumes[currentSprite->currentCostume].isSVG;
            calculateRenderPosition(currentSprite, isSVG);
            image->renderRect.x = currentSprite->renderInfo.renderX;
            image->renderRect.y = currentSprite->renderInfo.renderY;

            image->setScale(currentSprite->renderInfo.renderScaleY);
            if (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT && currentSprite->rotation < 0) {
                flip = SDL_FLIP_HORIZONTAL;
                image->renderRect.x += (currentSprite->spriteWidth * (isSVG ? 2 : 1)) * 1.125; // Don't ask why I'm multiplying by 1.125 here, I also have no idea, but it makes it work so...
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
                    SDL_RenderCopyEx(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                                     Math::radiansToDegrees(currentSprite->renderInfo.renderRotation), nullptr, flip);

                    // render another, blended image on top
                    SDL_SetTextureBlendMode(image->spriteTexture, SDL_BLENDMODE_ADD);
                    SDL_SetTextureAlphaMod(image->spriteTexture, (Uint8)(brightness * 255 * (alpha / 255.0f)));
                    SDL_RenderCopyEx(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                                     Math::radiansToDegrees(currentSprite->renderInfo.renderRotation), nullptr, flip);

                    // reset for next frame
                    SDL_SetTextureBlendMode(image->spriteTexture, SDL_BLENDMODE_BLEND);
                } else {
                    Uint8 col = static_cast<Uint8>(255 * (1.0f + brightness));
                    SDL_SetTextureColorMod(image->spriteTexture, col, col, col);

                    SDL_RenderCopyEx(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                                     Math::radiansToDegrees(currentSprite->renderInfo.renderRotation), nullptr, flip);
                    // reset for next frame
                    SDL_SetTextureColorMod(image->spriteTexture, 255, 255, 255);
                }
            } else {
                // if no brightness just render normal image
                SDL_SetTextureColorMod(image->spriteTexture, 255, 255, 255);
                SDL_RenderCopyEx(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                                 Math::radiansToDegrees(currentSprite->renderInfo.renderRotation), nullptr, flip);
            }
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

    SDL_RenderPresent(renderer);
    Image::FlushImages();
    SoundPlayer::flushAudio();
}

std::unordered_map<std::string, TextObject *> Render::monitorTexts;

void Render::renderPenLayer() {
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

    SDL_RenderCopy(renderer, penTexture, NULL, &renderRect);
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
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
            case SDL_WINDOWEVENT_RESIZED:
                SDL_GetWindowSizeInPixels(window, &windowWidth, &windowHeight);
                setRenderScale();

                if (!Scratch::hqpen) break;

                SDL_Texture *newTexture;
                if (Scratch::projectWidth / static_cast<double>(windowWidth) < Scratch::projectHeight / static_cast<double>(windowHeight))
                    newTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, Scratch::projectWidth * (windowHeight / static_cast<double>(Scratch::projectHeight)), windowHeight);
                else
                    newTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, windowWidth, Scratch::projectHeight * (windowWidth / static_cast<double>(Scratch::projectWidth)));

                SDL_SetTextureBlendMode(newTexture, SDL_BLENDMODE_NONE);
                SDL_SetTextureBlendMode(penTexture, SDL_BLENDMODE_NONE);
                SDL_SetRenderTarget(renderer, newTexture);
                SDL_RenderCopy(renderer, penTexture, nullptr, nullptr);
                SDL_SetRenderTarget(renderer, nullptr);
                SDL_SetTextureBlendMode(newTexture, SDL_BLENDMODE_BLEND);
                SDL_DestroyTexture(penTexture);
                penTexture = newTexture;

                break;
            }
            break;
        }
    }
    return true;
}
