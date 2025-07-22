#include "../scratch/render.hpp"
#include "interpret.hpp"
#include "render.hpp"

#ifdef __WIIU__
#include <coreinit/debug.h>
#include <nn/act.h>
#include <romfs-wiiu.h>
#include <whb/sdcard.h>
#endif

#ifdef __SWITCH__
#include <switch.h>

char nickname[0x21];
#endif

int windowWidth = 480;
int windowHeight = 360;
SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;

Render::RenderModes Render::renderMode = Render::TOP_SCREEN_ONLY;

// TODO: properly export these to input.cpp
SDL_GameController *controller;
bool touchActive = false;
SDL_Point touchPosition;

bool Render::Init() {
#ifdef __WIIU__
    if (romfsInit()) {
        OSFatal("Failed to init romfs.");
        return false;
    }
    if (!WHBMountSdCard()) {
        OSFatal("Failed to mount sd card.");
        return false;
    }
    nn::act::Initialize();
#elif defined(__SWITCH__)
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);

    AccountUid userID = {0};
    AccountProfile profile;
    AccountProfileBase profilebase;
    memset(&profilebase, 0, sizeof(profilebase));

    Result rc = romfsInit();
    if (R_FAILED(rc)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_SYSTEM, "Failed to init romfs."); // TODO: Include error code
        goto postAccount;
    }

    rc = accountInitialize(AccountServiceType_Application);
    if (R_FAILED(rc)) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "accountInitialize failed.");
        goto postAccount;
    }

    rc = accountGetPreselectedUser(&userID);
    if (R_FAILED(rc)) {
        PselUserSelectionSettings settings;
        memset(&settings, 0, sizeof(settings));
        rc = pselShowUserSelector(&userID, &settings);
        if (R_FAILED(rc)) {
            SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "pselShowUserSelector failed.");
            goto postAccount;
        }
    }

    rc = accountGetProfile(&profile, userID);
    if (R_FAILED(rc)) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "accountGetProfile failed.");
        goto postAccount;
    }

    rc = accountProfileGet(&profile, NULL, &profilebase);
    if (R_FAILED(rc)) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "accountProfileGet failed.");
        goto postAccount;
    }

    memset(nickname, 0, sizeof(nickname));
    strncpy(nickname, profilebase.nickname, sizeof(nickname) - 1);

    accountProfileClose(&profile);
    accountExit();
postAccount:
#endif

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    window = SDL_CreateWindow("Scratch Runtime", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (SDL_NumJoysticks() > 0) controller = SDL_GameControllerOpen(0);

    return true;
}
void Render::deInit() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

#if defined(__WIIU__) || defined(__SWITCH__)
    romfsExit();
#endif
#ifdef __WIIU__
    WHBUnmountSdCard();
    nn::act::Finalize();
#endif
}
void Render::renderSprites() {
    SDL_GetWindowSizeInPixels(window, &windowWidth, &windowHeight);
    // SDL_SetWindowSize(window,Scratch::projectWidth,Scratch::projectHeight);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    double scaleX = static_cast<double>(windowWidth) / Scratch::projectWidth;
    double scaleY = static_cast<double>(windowHeight) / Scratch::projectHeight;
    double scale;
    scale = std::min(scaleX, scaleY);

    // Sort sprites by layer first
    std::vector<Sprite *> spritesByLayer = sprites;
    std::sort(spritesByLayer.begin(), spritesByLayer.end(),
              [](const Sprite *a, const Sprite *b) {
                  return a->layer < b->layer;
              });

    for (Sprite *currentSprite : spritesByLayer) {
        if (!currentSprite->visible) continue;

        bool legacyDrawing = false;
        auto imgFind = images.find(currentSprite->costumes[currentSprite->currentCostume].id);
        if (imgFind == images.end()) {
            legacyDrawing = true;
        }
        if (!legacyDrawing) {
            SDL_Image *image = imgFind->second;
            SDL_RendererFlip flip = SDL_FLIP_NONE;

            image->setScale((currentSprite->size * 0.01) * scale / 2.0f);
            currentSprite->spriteWidth = image->textureRect.w / 2;
            currentSprite->spriteHeight = image->textureRect.h / 2;
            image->renderRect.x = currentSprite->xPosition;
            image->renderRect.y = currentSprite->yPosition;
            image->setRotation(Math::degreesToRadians(currentSprite->rotation - 90.0f));

            if (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT) {
                if (image->rotation < 0) {
                    flip = SDL_FLIP_HORIZONTAL;
                }
                image->setRotation(0);
            }
            if (currentSprite->rotationStyle == currentSprite->NONE) {
                image->setRotation(0);
            }

            image->renderRect.x = (currentSprite->xPosition * scale) + (windowWidth / 2) - (image->renderRect.w / 2);
            image->renderRect.y = (currentSprite->yPosition * -scale) + (windowHeight / 2) - (image->renderRect.h / 2);
            SDL_Point center = {image->renderRect.w / 2, image->renderRect.h / 2};

            SDL_RenderCopyEx(renderer, image->spriteTexture, &image->textureRect, &image->renderRect, image->rotation, &center, flip);
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

    SDL_RenderPresent(renderer);
}

bool Render::appShouldRun() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            return false;
            break;
        case SDL_CONTROLLERDEVICEADDED:
            controller = SDL_GameControllerOpen(0);
            break;
        case SDL_FINGERDOWN:
            touchActive = true;
            touchPosition = {event.tfinger.x * windowWidth,
                             event.tfinger.y * windowHeight};
            break;
        case SDL_FINGERMOTION:
            touchPosition = {event.tfinger.x * windowWidth,
                             event.tfinger.y * windowHeight};
            break;
        case SDL_FINGERUP:
            touchActive = false;
            break;
        }
    }
    return true;
}

// TODO create functionality for these in the SDL version.
// Would probably need to share more code between the two
// versions first (eg; text renderer for SDL version)

void LoadingScreen::init() {
}
void LoadingScreen::renderLoadingScreen() {
}
void LoadingScreen::cleanup() {
}
// or these,,,
void MainMenu::init() {
}
void MainMenu::render() {
}
void MainMenu::cleanup() {
}
