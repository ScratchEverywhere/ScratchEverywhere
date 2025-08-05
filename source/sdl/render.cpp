#include "../scratch/render.hpp"
#include "../scratch/audio.hpp"
#include "../scratch/input.hpp"
#include "../scratch/unzip.hpp"
#include "interpret.hpp"
#include "render.hpp"

#ifdef __WIIU__
#include <coreinit/debug.h>
#include <nn/act.h>
#include <romfs-wiiu.h>
#include <whb/log_udp.h>
#include <whb/sdcard.h>
#endif

int windowWidth = 480;
int windowHeight = 360;
SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;

Render::RenderModes Render::renderMode = Render::TOP_SCREEN_ONLY;
std::vector<Monitor> Render::visibleVariables;

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
#endif

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    // Initialize SDL_mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        Log::logWarning(std::string("SDL_Mixer could not initialize! ") + Mix_GetError());
        return false;
    }
    int flags = MIX_INIT_MP3 | MIX_INIT_OGG;
    if (Mix_Init(flags) != flags) {
        Log::logWarning(std::string("SDL_Mixer could not initialize MP3/OGG Support! ") + Mix_GetError());
    }
    TTF_Init();
    window = SDL_CreateWindow("Scratch Runtime", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (SDL_NumJoysticks() > 0) controller = SDL_GameControllerOpen(0);

    return true;
}
void Render::deInit() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SoundPlayer::cleanupAudio();
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
            if (image->isSVG) image->setScale(image->scale * 2);
            currentSprite->spriteWidth = image->textureRect.w / 2;
            currentSprite->spriteHeight = image->textureRect.h / 2;
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

            // ghost effect
            float ghost = std::clamp(currentSprite->ghostEffect, 0.0f, 100.0f);
            Uint8 alpha = static_cast<Uint8>(255 * (1.0f - ghost / 100.0f));
            SDL_SetTextureAlphaMod(image->spriteTexture, alpha);

            SDL_RenderCopyEx(renderer, image->spriteTexture, &image->textureRect, &image->renderRect, Math::radiansToDegrees(renderRotation), &center, flip);
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
                monitorTexts[var.id]->setRenderer(renderer);
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
                monitorTexts.erase(var.id);
            }
        }
    }
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

void LoadingScreen::init() {
}
void LoadingScreen::renderLoadingScreen() {
}
void LoadingScreen::cleanup() {
}

void MainMenu::init() {

    std::vector<std::string> projectFiles;
#ifdef __WIIU__
    projectFiles = Unzip::getProjectFiles(std::string(WHBGetSdCardMountPath()) + "/wiiu/scratch-wiiu/");
#else
    projectFiles = Unzip::getProjectFiles(".");
#endif

    int yPosition = 120;
    for (std::string &file : projectFiles) {
        TextObject *text = createTextObject(file, 0, yPosition);
        text->setRenderer(renderer);
        text->setColor(0xFF000000);
        text->y -= text->getSize()[1] / 2;
        if (text->getSize()[0] > windowWidth) {
            float scale = (float)windowWidth / (text->getSize()[0] * 1.15);
            text->setScale(scale);
        }
        projectTexts.push_back(text);
        yPosition += 50;
    }

    if (projectFiles.size() == 0) {
        std::string errorText;
#ifdef __WIIU__
        errorText = "No Scratch projects found!\n Go download a Scratch project and put it\n in sdcard:/wiiu/scratch-wiiu!\nPress Start to exit.";
#else
        errorText = "No Scratch projects found!\n Go download a Scratch project and put it\n in the same folder as this executable!\nPress Start to exit.";
#endif
        errorTextInfo = createTextObject(errorText,
                                         windowWidth / 2, windowWidth / 2);
        errorTextInfo->setRenderer(renderer);
        errorTextInfo->setScale(0.6);
        hasProjects = false;
    } else {
        selectedText = projectTexts.front();
        hasProjects = true;
    }
}
void MainMenu::render() {

    // use scratch input instead of direct SDL input because uhhhh lazy 😁
    Input::getInput();
    bool upPressed = std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "up arrow") != Input::inputButtons.end() && Input::keyHeldFrames < 2;
    bool downPressed = std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "down arrow") != Input::inputButtons.end() && Input::keyHeldFrames < 2;
    bool aPressed = std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "a") != Input::inputButtons.end() && Input::keyHeldFrames < 2;
    bool startPressed = std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "1") != Input::inputButtons.end() && Input::keyHeldFrames < 2;

    if (hasProjects) {

        if (downPressed) {
            if (selectedTextIndex < (int)projectTexts.size() - 1) {
                selectedTextIndex++;
                selectedText = projectTexts[selectedTextIndex];
            }
        }
        if (upPressed) {
            if (selectedTextIndex > 0) {
                selectedTextIndex--;
                selectedText = projectTexts[selectedTextIndex];
            }
        }
        cameraY = selectedText->y;
        cameraX = windowWidth / 2;

        if (aPressed) {
            Unzip::filePath = selectedText->getText();
        }
    } else {

        if (startPressed) {
            shouldExit = true;
        }
    }

    // begin frame
    SDL_GetWindowSizeInPixels(window, &windowWidth, &windowHeight);
    SDL_SetRenderDrawColor(renderer, 71, 107, 115, 255);
    SDL_RenderClear(renderer);

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapsed = now - logoStartTime;

    for (TextObject *text : projectTexts) {
        if (text == nullptr) continue;

        if (selectedText == text)
            text->setColor(0xFFFFFFFF);
        else
            text->setColor(0x000000FF);

        text->render(text->x + cameraX, text->y - (cameraY - (windowHeight / 2)));
    }

    if (errorTextInfo != nullptr) {
        errorTextInfo->render(errorTextInfo->x, errorTextInfo->y);
    }

    SDL_RenderPresent(renderer);
    SDL_Delay(16);
}
void MainMenu::cleanup() {
    for (TextObject *text : projectTexts) {
        delete text;
    }
    projectTexts.clear();

    selectedText = nullptr;
    if (errorTextInfo) delete errorTextInfo;

    SDL_GetWindowSizeInPixels(window, &windowWidth, &windowHeight);
    SDL_SetRenderDrawColor(renderer, 71, 107, 115, 255);
    SDL_RenderClear(renderer);

    SDL_RenderPresent(renderer);
    SDL_Delay(16);
}
