#include "../scratch/render.hpp"
#include "interpret.hpp"
#include "render.hpp"

#ifdef __WIIU__
#include <coreinit/debug.h>
#include <nn/act.h>
#include <romfs-wiiu.h>
#include <whb/sdcard.h>
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
#endif

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    // Initialize SDL_mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cout << "SDL_mixer could not initialize! SDL_mixer Error: " << Mix_GetError() << std::endl;
        return false;
    }
    int flags = MIX_INIT_MP3 | MIX_INIT_OGG;
    if (Mix_Init(flags) != flags) {
        std::cout << "SDL_mixer could not initialize MP3/OGG support! SDL_mixer Error: " << Mix_GetError() << std::endl;
    }
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

#ifdef __WIIU__
    romfsExit();
    WHBUnmountSdCard();
    nn::act::Finalize();
#endif
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
        SDL_Rect leftBar = {0, 0, static_cast<int>(barWidth), screenHeight};
        SDL_Rect rightBar = {static_cast<int>(screenWidth - barWidth), 0, static_cast<int>(barWidth), screenHeight};

        SDL_RenderFillRect(renderer, &leftBar);
        SDL_RenderFillRect(renderer, &rightBar);
    } else if (screenAspect < projectAspect) {
        // Horizontal bars,,,
        float scale = static_cast<float>(screenWidth) / Scratch::projectWidth;
        float scaledProjectHeight = Scratch::projectHeight * scale;
        float barHeight = (screenHeight - scaledProjectHeight) / 2.0f;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_Rect topBar = {0, 0, screenWidth, static_cast<int>(barHeight)};
        SDL_Rect bottomBar = {0, static_cast<int>(screenHeight - barHeight), screenWidth, static_cast<int>(barHeight)};

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
        } else {
            currentSprite->rotationCenterX = currentSprite->costumes[currentSprite->currentCostume].rotationCenterX;
            currentSprite->rotationCenterY = currentSprite->costumes[currentSprite->currentCostume].rotationCenterY;
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

            double rotationCenterX = ((((currentSprite->rotationCenterX - currentSprite->spriteWidth)) / 2) * scale);
            double rotationCenterY = ((((currentSprite->rotationCenterY - currentSprite->spriteHeight)) / 2) * scale);

            image->renderRect.x = ((currentSprite->xPosition * scale) + (windowWidth / 2) - (image->renderRect.w / 2)) - rotationCenterX;
            image->renderRect.y = ((currentSprite->yPosition * -scale) + (windowHeight / 2) - (image->renderRect.h / 2)) - rotationCenterY;
            SDL_Point center = {image->renderRect.w / 2, image->renderRect.h / 2};

            // ghost effect
            float ghost = std::clamp(currentSprite->ghostEffect, 0.0f, 100.0f);
            Uint8 alpha = static_cast<Uint8>(255 * (1.0f - ghost / 100.0f));
            SDL_SetTextureAlphaMod(image->spriteTexture, alpha);

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

    drawBlackBars(windowWidth, windowHeight);

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
