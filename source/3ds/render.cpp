#include "render.hpp"
#include "../scratch/audio.hpp"
#include "../scratch/image.hpp"
#include "../scratch/input.hpp"
#include "../scratch/os.hpp"
#include "../scratch/render.hpp"
#include "../scratch/text.hpp"
#include "../scratch/unzip.hpp"
#include "image.hpp"
#include "interpret.hpp"
#include "spriteSheet.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#define SCREEN_WIDTH 400
#define BOTTOM_SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

C3D_RenderTarget *topScreen = nullptr;
C3D_RenderTarget *bottomScreen = nullptr;
u32 clrWhite = C2D_Color32f(1, 1, 1, 1);
u32 clrBlack = C2D_Color32f(0, 0, 0, 1);
u32 clrGreen = C2D_Color32f(0, 0, 1, 1);
u32 clrScratchBlue = C2D_Color32(71, 107, 115, 255);
std::chrono::_V2::system_clock::time_point startTime = std::chrono::high_resolution_clock::now();
std::chrono::_V2::system_clock::time_point endTime = std::chrono::high_resolution_clock::now();

Render::RenderModes Render::renderMode = Render::TOP_SCREEN_ONLY;

bool Render::Init() {
    gfxInitDefault();
    hidScanInput();
    u32 kDown = hidKeysHeld();
    if (kDown & KEY_SELECT) consoleInit(GFX_BOTTOM, NULL);
    osSetSpeedupEnable(true);

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    topScreen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    romfsInit();
    // waiting for beta 12 to enable,,
    SDL_Init(SDL_INIT_AUDIO);
    // Initialize SDL_mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        Log::logWarning(std::string("SDL_mixer could not initialize! Error: ") + Mix_GetError());
        // not returning false since emulators by default will error here
    }
    int flags = MIX_INIT_MP3 | MIX_INIT_OGG;
    if (Mix_Init(flags) != flags) {
        Log::logWarning(std::string("SDL_mixer could not initialize MP3/OGG support! SDL_mixer Error: ") + Mix_GetError());
    }

    return true;
}

bool Render::appShouldRun() {
    return aptMainLoop();
}

void drawBlackBars(int screenWidth, int screenHeight) {
    float screenAspect = static_cast<float>(screenWidth) / screenHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;

    if (screenAspect > projectAspect) {
        // Screen is wider than project,, vertical bars
        float scale = static_cast<float>(screenHeight) / Scratch::projectHeight;
        float scaledProjectWidth = Scratch::projectWidth * scale;
        float barWidth = (screenWidth - scaledProjectWidth) / 2.0f;

        C2D_DrawRectSolid(0, 0, 0.5f, barWidth, screenHeight, clrBlack);                      // Left bar
        C2D_DrawRectSolid(screenWidth - barWidth, 0, 0.5f, barWidth, screenHeight, clrBlack); // Right bar

    } else if (screenAspect < projectAspect) {
        // Screen is taller than project,, horizontal bars
        float scale = static_cast<float>(screenWidth) / Scratch::projectWidth;
        float scaledProjectHeight = Scratch::projectHeight * scale;
        float barHeight = (screenHeight - scaledProjectHeight) / 2.0f;

        C2D_DrawRectSolid(0, 0, 0.5f, screenWidth, barHeight, clrBlack);                        // Top bar
        C2D_DrawRectSolid(0, screenHeight - barHeight, 0.5f, screenWidth, barHeight, clrBlack); // Bottom bar
    }
}

void renderImage(C2D_Image *image, Sprite *currentSprite, std::string costumeId, bool bottom = false) {

    if (!currentSprite || currentSprite == nullptr) return;

    bool legacyDrawing = true;
    double screenOffset = (bottom && Render::renderMode != Render::BOTTOM_SCREEN_ONLY) ? -SCREEN_HEIGHT : 0;
    bool imageLoaded = false;
    for (Image::ImageRGBA rgba : Image::imageRGBAS) {
        if (rgba.name == costumeId) {

            legacyDrawing = false;
            currentSprite->spriteWidth = rgba.width / 2;
            currentSprite->spriteHeight = rgba.height / 2;

            if (imageC2Ds.find(costumeId) == imageC2Ds.end() || image->tex == nullptr || image->subtex == nullptr) {

                auto rgbaFind = std::find_if(Image::imageRGBAS.begin(), Image::imageRGBAS.end(),
                                             [&](const Image::ImageRGBA &rgba) { return rgba.name == costumeId; });

                if (rgbaFind != Image::imageRGBAS.end()) {
                    imageLoaded = queueC2DImage(*rgbaFind);
                } else {
                    imageLoaded = false;
                }

            } else imageLoaded = true;

            break;
        }
    }
    if (!imageLoaded) {
        legacyDrawing = true;
        currentSprite->spriteWidth = 64;
        currentSprite->spriteHeight = 64;
    }

    // double maxLayer = getMaxSpriteLayer();
    double scaleX = static_cast<double>(SCREEN_WIDTH) / Scratch::projectWidth;
    double scaleY = static_cast<double>(SCREEN_HEIGHT) / Scratch::projectHeight;
    double spriteSizeX = currentSprite->size * 0.01;
    double spriteSizeY = currentSprite->size * 0.01;
    double scale;
    double heightMultiplier = 0.5;
    int screenWidth = SCREEN_WIDTH;
    if (bottom) screenWidth = BOTTOM_SCREEN_WIDTH;
    if (Render::renderMode == Render::BOTH_SCREENS) {
        scaleY = static_cast<double>(SCREEN_HEIGHT) / (Scratch::projectHeight / 2.0);
        heightMultiplier = 1.0;
    }
    scale = bottom ? 1.0 : std::min(scaleX, scaleY);

    if (!legacyDrawing) {
        imageC2Ds[costumeId].freeTimer = 240;
        double rotation = Math::degreesToRadians(currentSprite->rotation - 90.0f);
        bool flipX = false;

        // check for rotation style
        if (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT) {
            if (std::cos(rotation) < 0) {
                spriteSizeX *= -1;
                flipX = true;
            }
            rotation = 0;
        }
        if (currentSprite->rotationStyle == currentSprite->NONE) {
            rotation = 0;
        }

        // Center the sprite's pivot point
        double rotationCenterX = ((((currentSprite->rotationCenterX - currentSprite->spriteWidth)) / 2) * scale);
        double rotationCenterY = ((((currentSprite->rotationCenterY - currentSprite->spriteHeight)) / 2) * scale);
        if (flipX) rotationCenterX -= currentSprite->spriteWidth;

        float alpha = 1.0f - (currentSprite->ghostEffect / 100.0f);
        C2D_ImageTint tinty;
        C2D_AlphaImageTint(&tinty, alpha);

        const double offsetX = rotationCenterX * spriteSizeX;
        const double offsetY = rotationCenterY * spriteSizeY;

        C2D_DrawImageAtRotated(
            imageC2Ds[costumeId].image,
            (currentSprite->xPosition * scale) + (screenWidth / 2) - offsetX * std::cos(rotation) + offsetY * std::sin(rotation),
            (currentSprite->yPosition * -1 * scale) + (SCREEN_HEIGHT * heightMultiplier) + screenOffset - offsetX * std::sin(rotation) - offsetY * std::cos(rotation),
            1,
            rotation,
            &tinty,
            (spriteSizeX)*scale / 2.0f,
            (spriteSizeY)*scale / 2.0f);
    } else {
        C2D_DrawRectSolid(
            (currentSprite->xPosition * scale) + (screenWidth / 2),
            (currentSprite->yPosition * -1 * scale) + (SCREEN_HEIGHT * heightMultiplier) + screenOffset,
            1,
            10 * scale,
            10 * scale,
            clrBlack);
    }

    // Draw collision points
    // auto collisionPoints = getCollisionPoints(currentSprite);
    // for (const auto &point : collisionPoints) {
    //     double screenOffset = bottom ? -SCREEN_HEIGHT : 0;      // Adjust for bottom screen
    //     double scale = bottom ? 1.0 : std::min(scaleX, scaleY); // Skip scaling if bottom is true

    //     C2D_DrawRectSolid(
    //         (point.first * scale) + (screenWidth / 2),
    //         (point.second * -1 * scale) + (SCREEN_HEIGHT * heightMultiplier) + screenOffset,
    //         1,         // Layer depth
    //         2 * scale, // Width of the rectangle
    //         2 * scale, // Height of the rectangle
    //         clrBlack);
    // }
    // Draw mouse pointer
    if (Input::mousePointer.isMoving)
        C2D_DrawRectSolid((Input::mousePointer.x * scale) + (screenWidth / 2),
                          (Input::mousePointer.y * -1 * scale) + (SCREEN_HEIGHT * heightMultiplier) + screenOffset, 1, 5, 5, clrGreen);

    currentSprite->lastCostumeId = costumeId;
}

void Render::renderSprites() {

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(topScreen, clrWhite);
    C2D_TargetClear(bottomScreen, clrWhite);

    if (Render::renderMode != Render::BOTTOM_SCREEN_ONLY) {
        C2D_SceneBegin(topScreen);

        // int times = 1;
        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

        // Sort sprites by layer (lowest to highest)
        std::vector<Sprite *> spritesByLayer = sprites;
        std::sort(spritesByLayer.begin(), spritesByLayer.end(),
                  [](const Sprite *a, const Sprite *b) {
                      return a->layer < b->layer;
                  });

        // Now render sprites in order from lowest to highest layer
        for (Sprite *currentSprite : spritesByLayer) {
            if (!currentSprite->visible) continue;

            // look through every costume in sprite for correct one
            int costumeIndex = 0;
            for (const auto &costume : currentSprite->costumes) {
                if (costumeIndex == currentSprite->currentCostume) {
                    currentSprite->rotationCenterX = costume.rotationCenterX;
                    currentSprite->rotationCenterY = costume.rotationCenterY;
                    renderImage(&imageC2Ds[costume.id].image, currentSprite, costume.id);
                    break;
                }
                costumeIndex++;
            }
        }
    }
    if (Render::renderMode != Render::BOTH_SCREENS)
        drawBlackBars(SCREEN_WIDTH, SCREEN_HEIGHT);

    if (Render::renderMode == Render::BOTH_SCREENS || Render::renderMode == Render::BOTTOM_SCREEN_ONLY) {
        C2D_SceneBegin(bottomScreen);
        // Sort sprites by layer (lowest to highest)
        std::vector<Sprite *> spritesByLayer = sprites;
        std::sort(spritesByLayer.begin(), spritesByLayer.end(),
                  [](const Sprite *a, const Sprite *b) {
                      return a->layer < b->layer;
                  });

        // Now render sprites in order from lowest to highest layer
        for (Sprite *currentSprite : spritesByLayer) {
            if (!currentSprite->visible) continue;

            // look through every costume in sprite for correct one
            int costumeIndex = 0;
            for (const auto &costume : currentSprite->costumes) {
                if (costumeIndex == currentSprite->currentCostume) {
                    currentSprite->rotationCenterX = costume.rotationCenterX;
                    currentSprite->rotationCenterY = costume.rotationCenterY;
                    renderImage(&imageC2Ds[costume.id].image, currentSprite, costume.id, true);
                    break;
                }
                costumeIndex++;
            }
        }
        if (Render::renderMode != Render::BOTH_SCREENS)
            drawBlackBars(BOTTOM_SCREEN_WIDTH, SCREEN_HEIGHT);
    }

    C2D_Flush();
    C3D_FrameEnd(0);
    Image::FlushImages();
    // gspWaitForVBlank();
    endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = endTime - startTime;
    // int FPS = 1000.0 / std::round(duration.count());
    // std::cout << "\x1b[8;0HCPU: " <<C3D_GetProcessingTime()*6.0f<<"\nGPU: "<< C3D_GetDrawingTime()*6.0f << "\nCmdBuf: " <<C3D_GetCmdBufUsage()*100.0f << "\nFPS: " << FPS <<  std::endl;
    startTime = std::chrono::high_resolution_clock::now();
    osSetSpeedupEnable(true);
}

void LoadingScreen::renderLoadingScreen() {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(topScreen, clrScratchBlue);
    C2D_SceneBegin(topScreen);

    // if(text != nullptr){
    // text->render();
    // }
    for (squareObject &square : squares) {
        square.y -= square.size * 0.1;
        if (square.x > 400 + square.size) square.x = 0 - square.size;
        if (square.y < 0 - square.size) square.y = 240 + square.size;
        C2D_DrawRectSolid(square.x, square.y, 1, square.size, square.size, C2D_Color32(255, 255, 255, 75));
    }

    C3D_FrameEnd(0);
}

void LoadingScreen::init() {
    // text = new TextObject("Loading...",200,120);
    createSquares(20);
}

void LoadingScreen::cleanup() {
    // if(text && text != nullptr)
    // delete text;
    squares.clear();

    C2D_Flush();
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(topScreen, clrBlack);
    C2D_SceneBegin(topScreen);

    C2D_TargetClear(bottomScreen, clrBlack);
    C2D_SceneBegin(bottomScreen);

    C3D_FrameEnd(0);
    gspWaitForVBlank();
}

SpriteSheetObject *logo;

void MainMenu::init() {

    std::vector<std::string> projectFiles = Unzip::getProjectFiles(".");

    int yPosition = 120;
    for (std::string &file : projectFiles) {
        TextObject *text = createTextObject(file, 0, yPosition);
        text->setColor(C2D_Color32f(0, 0, 0, 1));
        text->y -= text->getSize()[1] / 2;
        if (text->getSize()[0] > BOTTOM_SCREEN_WIDTH) {
            float scale = (float)BOTTOM_SCREEN_WIDTH / (text->getSize()[0] * 1.15);
            text->setScale(scale);
        }
        projectTexts.push_back(text);
        yPosition += 50;
    }

    if (projectFiles.size() == 0) {
        errorTextInfo = createTextObject("No Scratch projects found!\n Go download a Scratch project and put it\n in the 3ds folder of your SD card!\nPress Start to exit.",
                                         BOTTOM_SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
        errorTextInfo->setScale(0.6);
        hasProjects = false;
    } else {
        selectedText = projectTexts.front();
        hasProjects = true;
    }

    logo = new SpriteSheetObject("romfs:/gfx/menuElements.t3x", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
    infoText = createTextObject("Runtime by NateXS", 340, 225);
    infoText->setScale(0.5);
}
void MainMenu::render() {

    if (hasProjects) {

        hidScanInput();
        u32 kJustPressed = hidKeysDown();

        if (kJustPressed & (KEY_DDOWN | KEY_CPAD_DOWN)) {
            if (selectedTextIndex < (int)projectTexts.size() - 1) {
                selectedTextIndex++;
                selectedText = projectTexts[selectedTextIndex];
            }
        }
        if (kJustPressed & (KEY_DUP | KEY_CPAD_UP)) {
            if (selectedTextIndex > 0) {
                selectedTextIndex--;
                selectedText = projectTexts[selectedTextIndex];
            }
        }
        cameraX = BOTTOM_SCREEN_WIDTH / 2;
        cameraY = selectedText->y;

        if (kJustPressed & KEY_A) {
            Unzip::filePath = selectedText->getText();
        }
    } else {
        hidScanInput();
        u32 kJustPressed = hidKeysDown();
        if (kJustPressed & (KEY_DDOWN | KEY_START)) {
            shouldExit = true;
        }
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(topScreen, clrScratchBlue);
    C2D_SceneBegin(topScreen);

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapsed = now - logoStartTime;

    float timeSeconds = elapsed.count();
    float bobbingOffset = std::sin(timeSeconds * 2.0f) * 5.0f; // speed * amplitude

    logo->render(logo->x, (SCREEN_HEIGHT / 2) + bobbingOffset);
    infoText->render(infoText->x, infoText->y);

    C2D_TargetClear(bottomScreen, clrScratchBlue);
    C2D_SceneBegin(bottomScreen);

    for (TextObject *text : projectTexts) {
        if (text == nullptr || text->y > 300) continue;

        if (selectedText == text)
            text->setColor(C2D_Color32(237, 223, 214, 255));
        else
            text->setColor(C2D_Color32(0, 0, 0, 255));

        text->render(text->x + cameraX, text->y - (cameraY - (SCREEN_HEIGHT / 2)));
    }

    if (errorTextInfo != nullptr) {
        errorTextInfo->render(errorTextInfo->x, errorTextInfo->y);
    }

    // C2D_Flush();
    C3D_FrameEnd(0);
    gspWaitForVBlank();
}
void MainMenu::cleanup() {
    for (TextObject *text : projectTexts) {
        delete text;
    }
    projectTexts.clear();

    delete logo;
    delete infoText;
    selectedText = nullptr;
    if (errorTextInfo) delete errorTextInfo;

    C2D_Flush();
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(topScreen, clrScratchBlue);
    C2D_SceneBegin(topScreen);

    C2D_TargetClear(bottomScreen, clrScratchBlue);
    C2D_SceneBegin(bottomScreen);

    C3D_FrameEnd(0);
    gspWaitForVBlank();
}

void Render::deInit() {

    C2D_Fini();
    C3D_Fini();
    for (auto &[id, data] : imageC2Ds) {
        if (data.image.tex) {
            C3D_TexDelete(data.image.tex);
            free(data.image.tex);
        }

        if (data.image.subtex) {
            free((Tex3DS_SubTexture *)data.image.subtex);
        }
    }
    Image::imageRGBAS.clear();
    SoundPlayer::cleanupAudio();
    SDL_Quit();
    romfsExit();
    gfxExit();
}
