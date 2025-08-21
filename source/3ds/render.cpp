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
#ifdef ENABLE_AUDIO
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#endif

#ifdef ENABLE_CLOUDVARS
#include <malloc.h>

#define SOC_ALIGN 0x1000
#define SOC_BUFFERSIZE 0x100000
#endif

#define SCREEN_WIDTH 400
#define BOTTOM_SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

C3D_RenderTarget *topScreen = nullptr;
C3D_RenderTarget *topScreenRightEye = nullptr;
C3D_RenderTarget *bottomScreen = nullptr;
u32 clrWhite = C2D_Color32f(1, 1, 1, 1);
u32 clrBlack = C2D_Color32f(0, 0, 0, 1);
u32 clrGreen = C2D_Color32f(0, 0, 1, 1);
u32 clrScratchBlue = C2D_Color32(71, 107, 115, 255);
std::chrono::_V2::system_clock::time_point Render::startTime = std::chrono::high_resolution_clock::now();
std::chrono::_V2::system_clock::time_point Render::endTime = std::chrono::high_resolution_clock::now();

Render::RenderModes Render::renderMode = Render::TOP_SCREEN_ONLY;
bool Render::hasFrameBegan;
static int currentScreen = 0;
std::vector<Monitor> Render::visibleVariables;

#ifdef ENABLE_CLOUDVARS
static uint32_t *SOC_buffer = NULL;
#endif

bool Render::Init() {
    gfxInitDefault();
    hidScanInput();
    u32 kDown = hidKeysHeld();
    if (kDown & KEY_SELECT) consoleInit(GFX_BOTTOM, NULL);
    osSetSpeedupEnable(true);

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    gfxSet3D(true);
    topScreen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    topScreenRightEye = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

#ifdef ENABLE_CLOUDVARS
    int ret;

    SOC_buffer = (uint32_t *)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    if (SOC_buffer == NULL) {
        Log::logError("memalign: failed to allocate");
    } else if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
        std::ostringstream err;
        err << "socInit: 0x" << std::hex << std::setw(8) << std::setfill('0') << ret;
        Log::logError(err.str());
    }
#endif

    romfsInit();
#ifdef ENABLE_AUDIO
    SDL_Init(SDL_INIT_AUDIO);
    int sampleRate = 15000;
    int bufferSize = 1024;
    int channels = 1;

    bool isNew3DS = false;
    APT_CheckNew3DS(&isNew3DS);

    if (isNew3DS) {
        sampleRate = 48000;
        bufferSize = 4096;
        channels = 2;
    }

    if (Mix_OpenAudio(sampleRate, MIX_DEFAULT_FORMAT, channels, bufferSize) < 0) {
        Log::logWarning(std::string("SDL_mixer could not initialize! Error: ") + Mix_GetError());
    }
    int flags = MIX_INIT_MP3 | MIX_INIT_OGG;
    if (Mix_Init(flags) != flags) {
        Log::logWarning(std::string("SDL_mixer could not initialize MP3/OGG support! SDL_mixer Error: ") + Mix_GetError());
    }
#endif

    return true;
}

bool Render::appShouldRun() {
    if (toExit) return false;
    if (!aptMainLoop()) {
        toExit = true;
        return false;
    }
    return true;
}

void *Render::getRenderer() {
    return nullptr;
}

int Render::getWidth() {
    if (currentScreen == 0)
        return SCREEN_WIDTH;
    else return BOTTOM_SCREEN_WIDTH;
}
int Render::getHeight() {
    return SCREEN_HEIGHT;
}

void Render::beginFrame(int screen, int colorR, int colorG, int colorB) {
    if (!hasFrameBegan) {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
        hasFrameBegan = true;
    }
    if (screen == 0) {
        currentScreen = 0;
        C2D_TargetClear(topScreen, C2D_Color32(colorR, colorG, colorB, 255));
        C2D_SceneBegin(topScreen);
    } else {
        currentScreen = 1;
        C2D_TargetClear(bottomScreen, C2D_Color32(colorR, colorG, colorB, 255));
        C2D_SceneBegin(bottomScreen);
    }
}

void Render::endFrame(bool shouldFlush) {
    C2D_Flush();
    C3D_FrameEnd(0);
    if (shouldFlush) Image::FlushImages();
    hasFrameBegan = false;
}

void Render::drawBox(int w, int h, int x, int y, int colorR, int colorG, int colorB, int colorA) {
    C2D_DrawRectSolid(
        x - (w / 2.0f),
        y - (h / 2.0f),
        1,
        w,
        h,
        C2D_Color32(colorR, colorG, colorB, colorA));
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

void renderImage(C2D_Image *image, Sprite *currentSprite, std::string costumeId, bool bottom = false, float x3DOffset = 0.0f) {

    if (!currentSprite || currentSprite == nullptr) return;

    bool legacyDrawing = true;
    bool isSVG = false;
    double screenOffset = (bottom && Render::renderMode != Render::BOTTOM_SCREEN_ONLY) ? -SCREEN_HEIGHT : 0;
    bool imageLoaded = false;
    for (imageRGBA rgba : imageRGBAS) {
        if (rgba.name == costumeId) {

            if (rgba.isSVG) isSVG = true;
            legacyDrawing = false;
            currentSprite->spriteWidth = rgba.width / 2;
            currentSprite->spriteHeight = rgba.height / 2;

            if (imageC2Ds.find(costumeId) == imageC2Ds.end() || image->tex == nullptr || image->subtex == nullptr) {

                auto rgbaFind = std::find_if(imageRGBAS.begin(), imageRGBAS.end(),
                                             [&](const imageRGBA &rgba) { return rgba.name == costumeId; });

                if (rgbaFind != imageRGBAS.end()) {
                    imageLoaded = get_C2D_Image(*rgbaFind);
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
    if (isSVG) {
        spriteSizeX *= 2;
        spriteSizeY *= 2;
    }
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
            static_cast<int>((currentSprite->xPosition * scale) + (screenWidth / 2) - offsetX * std::cos(rotation) + offsetY * std::sin(rotation)) + x3DOffset,
            static_cast<int>((currentSprite->yPosition * -1 * scale) + (SCREEN_HEIGHT * heightMultiplier) + screenOffset - offsetX * std::sin(rotation) - offsetY * std::cos(rotation)),
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
    C2D_TargetClear(topScreenRightEye, clrWhite);
    C2D_TargetClear(bottomScreen, clrWhite);

    float slider = osGet3DSliderState();
    const float depthScale = 12.0f / sprites.size();

    std::vector<Sprite *> spritesByLayer = sprites;
    std::sort(spritesByLayer.begin(), spritesByLayer.end(),
              [](const Sprite *a, const Sprite *b) {
                  return a->layer < b->layer;
              });

    // ---------- LEFT EYE ----------
    if (Render::renderMode != Render::BOTTOM_SCREEN_ONLY) {
        C2D_SceneBegin(topScreen);
        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

        for (size_t i = 0; i < spritesByLayer.size(); i++) {
            Sprite *currentSprite = spritesByLayer[i];
            if (!currentSprite->visible) continue;

            int costumeIndex = 0;
            for (const auto &costume : currentSprite->costumes) {
                if (costumeIndex == currentSprite->currentCostume) {
                    currentSprite->rotationCenterX = costume.rotationCenterX;
                    currentSprite->rotationCenterY = costume.rotationCenterY;

                    size_t totalSprites = spritesByLayer.size();
                    float eyeOffset = -slider * (static_cast<float>(totalSprites - 1 - i) * depthScale);

                    renderImage(&imageC2Ds[costume.id].image,
                                currentSprite,
                                costume.id,
                                false,
                                eyeOffset);
                    break;
                }
                costumeIndex++;
            }
        }
        renderVisibleVariables();
    }

    if (Render::renderMode != Render::BOTH_SCREENS)
        drawBlackBars(SCREEN_WIDTH, SCREEN_HEIGHT);

    // ---------- RIGHT EYE ----------
    if (slider > 0.0f && Render::renderMode != Render::BOTTOM_SCREEN_ONLY) {
        C2D_SceneBegin(topScreenRightEye);
        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

        for (size_t i = 0; i < spritesByLayer.size(); i++) {
            Sprite *currentSprite = spritesByLayer[i];
            if (!currentSprite->visible) continue;

            int costumeIndex = 0;
            for (const auto &costume : currentSprite->costumes) {
                if (costumeIndex == currentSprite->currentCostume) {
                    currentSprite->rotationCenterX = costume.rotationCenterX;
                    currentSprite->rotationCenterY = costume.rotationCenterY;

                    size_t totalSprites = spritesByLayer.size();
                    float eyeOffset = slider * (static_cast<float>(totalSprites - 1 - i) * depthScale);

                    renderImage(&imageC2Ds[costume.id].image,
                                currentSprite,
                                costume.id,
                                false,
                                eyeOffset);
                    break;
                }
                costumeIndex++;
            }
        }
        renderVisibleVariables();
    }

    if (Render::renderMode != Render::BOTH_SCREENS)
        drawBlackBars(SCREEN_WIDTH, SCREEN_HEIGHT);

    // ---------- BOTTOM SCREEN ----------
    if (Render::renderMode == Render::BOTH_SCREENS || Render::renderMode == Render::BOTTOM_SCREEN_ONLY) {
        C2D_SceneBegin(bottomScreen);

        for (size_t i = 0; i < spritesByLayer.size(); i++) {
            Sprite *currentSprite = spritesByLayer[i];
            if (!currentSprite->visible) continue;

            int costumeIndex = 0;
            for (const auto &costume : currentSprite->costumes) {
                if (costumeIndex == currentSprite->currentCostume) {
                    currentSprite->rotationCenterX = costume.rotationCenterX;
                    currentSprite->rotationCenterY = costume.rotationCenterY;

                    renderImage(&imageC2Ds[costume.id].image,
                                currentSprite,
                                costume.id,
                                true,
                                0.0f);
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
    osSetSpeedupEnable(true);
}

std::unordered_map<std::string, TextObject *> Render::monitorTexts;

void Render::renderVisibleVariables() {

    // get screen scale
    double scaleX = static_cast<double>(SCREEN_WIDTH) / Scratch::projectWidth;
    double scaleY = static_cast<double>(SCREEN_HEIGHT) / Scratch::projectHeight;
    double scale = std::min(scaleX, scaleY);

    // calculate black bar offset
    float screenAspect = static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;
    float barOffsetX = 0.0f;
    float barOffsetY = 0.0f;
    if (screenAspect > projectAspect) {
        float scaledProjectWidth = Scratch::projectWidth * scale;
        barOffsetX = (SCREEN_WIDTH - scaledProjectWidth) / 2.0f;
    } else if (screenAspect < projectAspect) {
        float scaledProjectHeight = Scratch::projectHeight * scale;
        barOffsetY = (SCREEN_HEIGHT - scaledProjectHeight) / 2.0f;
    }

    for (auto &var : visibleVariables) {
        if (var.visible) {

            std::string renderText = BlockExecutor::getMonitorValue(var).asString();

            if (monitorTexts.find(var.id) == monitorTexts.end()) {
                monitorTexts[var.id] = createTextObject(renderText, var.x, var.y);
            } else {
                monitorTexts[var.id]->setText(renderText);
            }
            monitorTexts[var.id]->setColor(C2D_Color32(0, 0, 0, 255));
            if (var.mode != "large") {
                monitorTexts[var.id]->setCenterAligned(false);
                monitorTexts[var.id]->setScale(0.6);
            } else {
                monitorTexts[var.id]->setCenterAligned(true);
                monitorTexts[var.id]->setScale(1);
            }

            monitorTexts[var.id]->render(var.x + barOffsetX, var.y + barOffsetY);

        } else {
            if (monitorTexts.find(var.id) != monitorTexts.end()) {
                monitorTexts.erase(var.id);
            }
        }
    }
}

void Render::deInit() {
#ifdef ENABLE_CLOUDVARS
    socExit();
#endif

    Image::cleanupImages();
    SoundPlayer::deinit();
    C2D_Fini();
    C3D_Fini();
#ifdef ENABLE_AUDIO
    SDL_Quit();
#endif
    romfsExit();
    gfxExit();
}
