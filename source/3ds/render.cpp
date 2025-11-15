#include "scratch/render.hpp"
#include "../scratch/audio.hpp"
#include "../scratch/blocks/pen.hpp"
#include "../scratch/interpret.hpp"
#include "blocks/pen.hpp"
#include "image.hpp"
#include "input.hpp"
#include "interpret.hpp"
#include "text.hpp"
#include "unzip.hpp"
#include <chrono>

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
std::chrono::system_clock::time_point Render::startTime = std::chrono::system_clock::now();
std::chrono::system_clock::time_point Render::endTime = std::chrono::system_clock::now();
std::unordered_map<std::string, TextObject *> Render::monitorTexts;
bool Render::debugMode = false;
static bool isConsoleInit = false;
float Render::renderScale = 1.0f;

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
    if (kDown & KEY_SELECT) {
        consoleInit(GFX_BOTTOM, NULL);
        debugMode = true;
        isConsoleInit = true;
    }
    osSetSpeedupEnable(true);

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    gfxSet3D(true);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
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
    if (currentScreen == 0 && renderMode != BOTTOM_SCREEN_ONLY)
        return SCREEN_WIDTH;
    else return BOTTOM_SCREEN_WIDTH;
}
int Render::getHeight() {
    return SCREEN_HEIGHT;
}

bool Render::initPen() {
    if (penRenderTarget != nullptr) return true;

    const int width = renderMode != BOTTOM_SCREEN_ONLY ? SCREEN_WIDTH : BOTTOM_SCREEN_WIDTH;
    const int height = renderMode != BOTH_SCREENS ? SCREEN_HEIGHT : SCREEN_HEIGHT * 2;

    // texture dimensions must be a power of 2. subtex dimensions can be the actual resolution.
    penTex = new C3D_Tex();
    penTex->width = Math::next_pow2(width);
    penTex->height = Math::next_pow2(height);
    penImage.tex = penTex;

    penSubtex.width = width;
    penSubtex.height = height;
    penSubtex.left = 0.0f;
    penSubtex.top = 0.0f;
    penSubtex.right = (float)penSubtex.width / (float)penTex->width;
    penSubtex.bottom = (float)penSubtex.height / (float)penTex->height;

    if (penSubtex.top < penSubtex.bottom) std::swap(penSubtex.top, penSubtex.bottom);

    penImage.subtex = &penSubtex;

    if (!C3D_TexInitVRAM(penImage.tex, penTex->width, penTex->height, GPU_RGBA8)) {
        penRenderTarget = nullptr;
        Log::logError("Failed to create pen texture.");
        return false;
    } else {
        penRenderTarget = C3D_RenderTargetCreateFromTex(penImage.tex, GPU_TEXFACE_2D, 0, GPU_RB_DEPTH16);
        C3D_TexSetFilter(penImage.tex, GPU_LINEAR, GPU_LINEAR);
        C2D_TargetClear(penRenderTarget, C2D_Color32(0, 0, 0, 0));
    }
    return true;
}

void Render::penMove(double x1, double y1, double x2, double y2, Sprite *sprite) {
    const ColorRGB rgbColor = CSB2RGB(sprite->penData.color);
    if (!Render::hasFrameBegan) {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        Render::hasFrameBegan = true;
    }
    C2D_SceneBegin(penRenderTarget);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

    const int width = getWidth();
    const int height = getHeight();

    const float heightMultiplier = 0.5f;
    const int transparency = 255 * (1 - sprite->penData.transparency / 100);
    const u32 color = C2D_Color32(rgbColor.r, rgbColor.g, rgbColor.b, transparency);
    const float thickness = sprite->penData.size * renderScale;

    const float x1_scaled = (x1 * renderScale) + (width / 2);
    const float y1_scaled = (y1 * -1 * renderScale) + (height * heightMultiplier) + TEXTURE_OFFSET;
    const float x2_scaled = (x2 * renderScale) + (width / 2);
    const float y2_scaled = (y2 * -1 * renderScale) + (height * heightMultiplier) + TEXTURE_OFFSET;

    C2D_DrawLine(x1_scaled, y1_scaled, color, x2_scaled, y2_scaled, color, thickness, 0);

    // Draw circles at both ends for smooth line caps
    const float radius = thickness / 2.0f;

    // Circle at start point
    C2D_DrawCircleSolid(x1_scaled, y1_scaled, 0, radius, color);

    // Circle at end point
    C2D_DrawCircleSolid(x2_scaled, y2_scaled, 0, radius, color);
}

void Render::beginFrame(int screen, int colorR, int colorG, int colorB) {
    if (!hasFrameBegan) {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        hasFrameBegan = true;
    }
    if (screen == 0) {
        currentScreen = 0;
        C2D_TargetClear(topScreen, C2D_Color32(colorR, colorG, colorB, 255));
        C2D_SceneBegin(topScreen);
    } else if (!isConsoleInit) {
        currentScreen = 1;
        C2D_TargetClear(bottomScreen, C2D_Color32(colorR, colorG, colorB, 255));
        C2D_SceneBegin(bottomScreen);
    } else {
        // render bottom screen content on top screen if logging is on the bottom screen
        currentScreen = 0;
        C2D_SceneBegin(topScreen);
    }
}

void Render::endFrame(bool shouldFlush) {
    C2D_Flush();
    C3D_FrameEnd(0);
    if (shouldFlush) Image::FlushImages();
    hasFrameBegan = false;
}

void Render::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
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

void renderImage(C2D_Image *image, Sprite *currentSprite, const std::string &costumeId, const bool &bottom = false, float xOffset = 0.0f, const int yOffset = 0) {
    if (!currentSprite || currentSprite == nullptr) return;

    bool isSVG = false;

    auto imageIt = images.find(costumeId);
    if (imageIt == images.end()) {
        currentSprite->spriteWidth = 64;
        currentSprite->spriteHeight = 64;
        return;
    }

    ImageData &data = imageIt->second;
    isSVG = data.isSVG;
    currentSprite->spriteWidth = data.width >> 1;
    currentSprite->spriteHeight = data.height >> 1;

    Render::calculateRenderPosition(currentSprite, isSVG);

    C2D_ImageTint tinty;

    // set ghost and brightness effect
    if (currentSprite->brightnessEffect != 0.0f || currentSprite->ghostEffect != 0.0f) {
        float brightnessEffect = currentSprite->brightnessEffect * 0.01f;
        float alpha = 255.0f * (1.0f - currentSprite->ghostEffect / 100.0f);
        if (brightnessEffect > 0)
            C2D_PlainImageTint(&tinty, C2D_Color32(255, 255, 255, alpha), brightnessEffect);
        else
            C2D_PlainImageTint(&tinty, C2D_Color32(0, 0, 0, alpha), brightnessEffect);
    } else C2D_AlphaImageTint(&tinty, 1.0f);

    C2D_DrawImageAtRotated(
        data.image,
        currentSprite->renderInfo.renderX + xOffset,
        currentSprite->renderInfo.renderY + yOffset,
        1,
        currentSprite->renderInfo.renderRotation,
        &tinty,
        currentSprite->renderInfo.renderScaleX,
        currentSprite->renderInfo.renderScaleY);
    data.freeTimer = data.maxFreeTimer;
}

void Render::renderSprites() {
    if (isConsoleInit) renderMode = RenderModes::TOP_SCREEN_ONLY;
    if (!Render::hasFrameBegan)
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    // Always start rendering top screen, otherwise bottom screen only rendering gets weird fsr
    C2D_SceneBegin(topScreen);

    float slider = osGet3DSliderState();
    const float depthScale = 8.0f / sprites.size();

    // ---------- LEFT EYE ----------
    if (Render::renderMode != Render::BOTTOM_SCREEN_ONLY) {
        C2D_TargetClear(topScreen, clrWhite);
        currentScreen = 0;

        size_t i = 0;
        for (auto it = sprites.rbegin(); it != sprites.rend(); ++it) {
            Sprite *currentSprite = *it;

            // render the pen texture above the backdrop, but below every other sprite
            if (i == 1 && penRenderTarget != nullptr) {
                const float yOffset = renderMode == BOTH_SCREENS ? 120.0f + TEXTURE_OFFSET : 0.0f;

                C2D_DrawImageAt(penImage,
                                0.0f,
                                yOffset,
                                0,
                                nullptr,
                                1.0f,
                                1.0f);
            }

            if (!currentSprite->visible) continue;

            int costumeIndex = 0;
            for (const auto &costume : currentSprite->costumes) {
                if (costumeIndex == currentSprite->currentCostume) {
                    currentSprite->rotationCenterX = costume.rotationCenterX;
                    currentSprite->rotationCenterY = costume.rotationCenterY;

                    size_t totalSprites = sprites.size();
                    float eyeOffset = -slider * (static_cast<float>(totalSprites - 1 - i) * depthScale);

                    renderImage(&images[costume.id].image,
                                currentSprite,
                                costume.id,
                                false,
                                eyeOffset,
                                renderMode == BOTH_SCREENS ? 120 : 0);
                    break;
                }
                costumeIndex++;
            }
            i++;
        }
        renderVisibleVariables();
        // Draw mouse pointer
        if (Input::mousePointer.isMoving) {
            C2D_DrawRectSolid((Input::mousePointer.x * renderScale) + (SCREEN_WIDTH * 0.5),
                              (Input::mousePointer.y * -1 * renderScale) + (SCREEN_HEIGHT * 0.5), 1, 5, 5, clrGreen);
            Input::mousePointer.x = std::clamp((float)Input::mousePointer.x, -Scratch::projectWidth * 0.5f, Scratch::projectWidth * 0.5f);
            Input::mousePointer.y = std::clamp((float)Input::mousePointer.y, -Scratch::projectHeight * 0.5f, Scratch::projectHeight * 0.5f);
        }
    }

    if (Render::renderMode != Render::BOTH_SCREENS)
        drawBlackBars(SCREEN_WIDTH, SCREEN_HEIGHT);

    // ---------- RIGHT EYE ----------
    if (slider > 0.0f && Render::renderMode != Render::BOTTOM_SCREEN_ONLY) {
        C2D_SceneBegin(topScreenRightEye);
        C2D_TargetClear(topScreenRightEye, clrWhite);
        currentScreen = 0;

        size_t i = 0;
        for (auto it = sprites.rbegin(); it != sprites.rend(); ++it) {
            Sprite *currentSprite = *it;

            // render the pen texture above the backdrop, but below every other sprite
            if (i == 1 && penRenderTarget != nullptr) {
                const float yOffset = renderMode == BOTH_SCREENS ? 120.0f + TEXTURE_OFFSET : 0.0f;

                C2D_DrawImageAt(penImage,
                                0.0f,
                                yOffset,
                                0,
                                nullptr,
                                1.0f,
                                1.0f);
            }

            if (!currentSprite->visible) continue;

            int costumeIndex = 0;
            for (const auto &costume : currentSprite->costumes) {
                if (costumeIndex == currentSprite->currentCostume) {
                    currentSprite->rotationCenterX = costume.rotationCenterX;
                    currentSprite->rotationCenterY = costume.rotationCenterY;

                    size_t totalSprites = sprites.size();
                    float eyeOffset = slider * (static_cast<float>(totalSprites - 1 - i) * depthScale);

                    renderImage(&images[costume.id].image,
                                currentSprite,
                                costume.id,
                                false,
                                eyeOffset,
                                renderMode == BOTH_SCREENS ? 120 : 0);
                    break;
                }
                costumeIndex++;
            }
            i++;
        }
        renderVisibleVariables();

        if (Render::renderMode != Render::BOTH_SCREENS)
            drawBlackBars(SCREEN_WIDTH, SCREEN_HEIGHT);
    }

    // ---------- BOTTOM SCREEN ----------
    if (Render::renderMode == Render::BOTH_SCREENS || Render::renderMode == Render::BOTTOM_SCREEN_ONLY) {
        C2D_SceneBegin(bottomScreen);
        C2D_TargetClear(bottomScreen, clrWhite);

        if (Render::renderMode != Render::BOTH_SCREENS)
            currentScreen = 1;

        size_t i = 0;
        for (auto it = sprites.rbegin(); it != sprites.rend(); ++it) {
            Sprite *currentSprite = *it;

            // render the pen texture above the backdrop, but below every other sprite
            if (i == 1 && penRenderTarget != nullptr) {
                const float yOffset = renderMode == BOTH_SCREENS ? -120.0f + TEXTURE_OFFSET : 0.0f;
                const float xOffset = renderMode == BOTH_SCREENS ? -(SCREEN_WIDTH - BOTTOM_SCREEN_WIDTH) * 0.5 : 0.0f;

                C2D_DrawImageAt(penImage,
                                xOffset,
                                yOffset,
                                0,
                                nullptr,
                                1.0f,
                                1.0f);
            }

            if (!currentSprite->visible) continue;

            int costumeIndex = 0;
            for (const auto &costume : currentSprite->costumes) {
                if (costumeIndex == currentSprite->currentCostume) {
                    currentSprite->rotationCenterX = costume.rotationCenterX;
                    currentSprite->rotationCenterY = costume.rotationCenterY;

                    renderImage(&images[costume.id].image,
                                currentSprite,
                                costume.id,
                                true,
                                renderMode == BOTH_SCREENS ? -(SCREEN_WIDTH - BOTTOM_SCREEN_WIDTH) * 0.5 : 0,
                                renderMode == BOTH_SCREENS ? -120 : 0);
                    break;
                }
                costumeIndex++;
            }
        }

        if (Render::renderMode != Render::BOTH_SCREENS) {
            drawBlackBars(BOTTOM_SCREEN_WIDTH, SCREEN_HEIGHT);
            renderVisibleVariables();
        }
    }

    C3D_FrameEnd(0);
    C2D_Flush();
    Image::FlushImages();
#ifdef ENABLE_AUDIO
    SoundPlayer::flushAudio();
#endif
    osSetSpeedupEnable(true);
    hasFrameBegan = false;
}

void Render::deInit() {
#ifdef ENABLE_CLOUDVARS
    socExit();
#endif

    if (penRenderTarget != nullptr) {
        C3D_RenderTargetDelete(penRenderTarget);
        C3D_TexDelete(penImage.tex);
    }

    Image::cleanupImages();
    SoundPlayer::cleanupAudio();
    TextObject::cleanupText();
    SoundPlayer::deinit();
    C2D_Fini();
    C3D_Fini();
    romfsExit();
    gfxExit();
}
