#include "renderer_c2d.hpp"
#include "image.hpp"
#include "speech_manager_c2d.hpp"
#include <audio.hpp>
#include <downloader.hpp>
#include <input.hpp>
#include <log.hpp>
#include <runtime.hpp>
#include <text.hpp>
#include <unzip.hpp>
#include <window.hpp>
#include <windowing/3ds/window_3ds.hpp>

static u8 is_2ds = CFGU_GetModelNintendo2DS(&is_2ds);

constexpr unsigned int screenWidth = 400;
constexpr unsigned int bottomScreenWidth = 320;
constexpr unsigned int screenHeight = 240;

static C3D_RenderTarget *topScreen = nullptr;
static C3D_RenderTarget *topScreenRightEye = nullptr;
static C3D_RenderTarget *bottomScreen = nullptr;
constexpr u32 clrWhite = C2D_Color32f(1, 1, 1, 1);
constexpr u32 clrBlack = C2D_Color32f(0, 0, 0, 1);
constexpr u32 clrGreen = C2D_Color32f(0, 0, 1, 1);
static bool isConsoleInit = false;

#define OBJ_OFFSET(value) (Render::renderMode == Render::BOTH_SCREENS) ? -value : 0.0f

static C2D_Image penImage;
static C3D_RenderTarget *penRenderTarget = nullptr;
static Tex3DS_SubTexture penSubtex;
static C3D_Tex *penTex;

static int currentScreen = 0;

namespace {
enum class FastPenType {
    DOT,
    LINE
};

struct FastPenData {
    FastPenType type;
    float x1, y1;
    float x2, y2;
    float size;
    u32 color;
};

std::vector<FastPenData> fastPenQueue;

void drawBlackBars(int screenWidth, int screenHeight) {
    float screenAspect = static_cast<float>(screenWidth) / screenHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;

    if (screenAspect == projectAspect) {
        return;
    }

    if (screenAspect > projectAspect) {
        // Vertical bars
        float scale = static_cast<float>(screenHeight) / Scratch::projectHeight;
        float scaledProjectWidth = Scratch::projectWidth * scale;
        float barWidth = (screenWidth - scaledProjectWidth) / 2.0f;

        C2D_DrawRectSolid(0, 0, 0.5f, barWidth, screenHeight, clrBlack);
        C2D_DrawRectSolid(screenWidth - barWidth, 0, 0.5f, barWidth, screenHeight, clrBlack);
        return;
    }

    // Horizontal bars
    float scale = static_cast<float>(screenWidth) / Scratch::projectWidth;
    float scaledProjectHeight = Scratch::projectHeight * scale;
    float barHeight = (screenHeight - scaledProjectHeight) / 2.0f;

    C2D_DrawRectSolid(0, 0, 0.5f, screenWidth, barHeight, clrBlack);
    C2D_DrawRectSolid(0, screenHeight - barHeight, 0.5f, screenWidth, barHeight, clrBlack);
}

void renderImage(Sprite *currentSprite, const std::string &costumeId, const bool &bottom = false, float xOffset = 0.0f, const int yOffset = 0) {
    if (!currentSprite || currentSprite == nullptr) return;

    auto imgFind = Scratch::costumeImages.find(costumeId);
    if (imgFind == Scratch::costumeImages.end()) {
        return;
    }

    Image *image = imgFind->second.get();
    const bool isSVG = currentSprite->costumes[currentSprite->currentCostume].isSVG;

    Render::calculateRenderPosition(currentSprite, isSVG);

    ImageRenderParams params;
    params.centered = true;
    params.x = currentSprite->renderInfo.renderX + xOffset;
    params.y = currentSprite->renderInfo.renderY + yOffset;
    params.rotation = currentSprite->renderInfo.renderRotation;
    params.scale = currentSprite->renderInfo.renderScaleY;
    params.flip = (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT && currentSprite->rotation < 0);
    params.opacity = 1.0f - (std::clamp(currentSprite->ghostEffect, 0.0f, 100.0f) * 0.01f);
    params.brightness = currentSprite->brightnessEffect;
    image->render(params);
}
} // namespace

Renderer_C2D::Renderer_C2D() {
    window = createWindow(400, 240, "Scratch Everywhere!", WindowingBackends::N3DS);
    if (window->error.has_value()) {
        error = "Failed to init window: " + window->error.value();
        delete window;
        window = nullptr;
        return;
    }

    hidScanInput();
    u32 kDown = hidKeysHeld();
    if (kDown & KEY_SELECT) {
        consoleInit(GFX_BOTTOM, NULL);
        isConsoleInit = true;
    }
    osSetSpeedupEnable(true);

    gfxSet3D(true);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

    topScreen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    topScreenRightEye = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
}

Renderer_C2D::~Renderer_C2D() {
    if (speechManager) {
        delete speechManager;
        speechManager = nullptr;
    }

    if (penRenderTarget != nullptr) {
        C3D_RenderTargetDelete(penRenderTarget);
        C3D_TexDelete(penImage.tex);
        penRenderTarget = nullptr;
    }
    TextObject::cleanupText();
    SoundPlayer::deinit();

    if (window) {
        window->cleanup();
        delete window;
        window = nullptr;
    }
}

bool Renderer_C2D::appShouldRun() {
    if (OS::toExit) return false;
    if (window && window->shouldClose()) {
        OS::toExit = true;
        return false;
    }
    return true;
}

void Renderer_C2D::setRenderTarget(void *renderTarget, int width, int height) {
    Log::logWarning("Renderer_C2D::setRenderTarget is not supported by the citro2d renderer");
}

void Renderer_C2D::clearRenderTarget() {
}

bool Renderer_C2D::createSpeechManager() {
    if (speechManager == nullptr) speechManager = new SpeechManagerC2D();
    return speechManager != nullptr;
}

void Renderer_C2D::destroySpeechManager() {
    delete speechManager;
    speechManager = nullptr;
}

SpeechManager *Renderer_C2D::getSpeechManager() {
    return speechManager;
}

int Renderer_C2D::getWidth() {
    if (currentScreen == 0 && Render::renderMode != Render::BOTTOM_SCREEN_ONLY)
        return screenWidth;
    else return bottomScreenWidth;
}

int Renderer_C2D::getHeight() {
    return screenHeight;
}

float Renderer_C2D::getPixelDensity() {
    return 1.0f;
}

bool Renderer_C2D::initPen() {
    if (penRenderTarget != nullptr) return true;

    const int width = Render::renderMode != Render::BOTTOM_SCREEN_ONLY ? screenWidth : bottomScreenWidth;
    const int height = Render::renderMode != Render::BOTH_SCREENS ? screenHeight : screenHeight * 2;

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
        Log::logCritical("Failed to create pen texture.", false);
        return false;
    } else {
        penRenderTarget = C3D_RenderTargetCreateFromTex(penImage.tex, GPU_TEXFACE_2D, 0, GPU_RB_DEPTH16);
        C3D_TexSetFilter(penImage.tex, GPU_LINEAR, GPU_LINEAR);
        penClear();
    }
    return true;
}

void Renderer_C2D::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    uint8_t alpha = static_cast<uint8_t>((100.0 - sprite->penData.color.transparency) / 100.0 * 255.0);

    const int width = getWidth();
    const int height = getHeight();

    const int PEN_Y_OFFSET = Render::renderMode != Render::BOTH_SCREENS ? 16 : (screenHeight * 0.5) + 32;

    const float heightMultiplier = 0.5f;
    const u32 color = C2D_Color32(rgbColor.r, rgbColor.g, rgbColor.b, alpha);
    const float thickness = sprite->penData.size * Render::renderScale;

    const float x1_scaled = (x1 * Render::renderScale) + (width / 2);
    const float y1_scaled = (y1 * -1 * Render::renderScale) + (height * heightMultiplier) + PEN_Y_OFFSET;
    const float x2_scaled = (x2 * Render::renderScale) + (width / 2);
    const float y2_scaled = (y2 * -1 * Render::renderScale) + (height * heightMultiplier) + PEN_Y_OFFSET;

    C2D_DrawLine(x1_scaled, y1_scaled, color, x2_scaled, y2_scaled, color, thickness, 0);
    fastPenQueue.push_back({FastPenType::LINE, x1_scaled, y1_scaled, x2_scaled, y2_scaled, thickness, color});
}

void Renderer_C2D::penDotFast(Sprite *sprite) {
    const int PEN_Y_OFFSET = Render::renderMode != Render::BOTH_SCREENS ? 16 : (screenHeight * 0.5) + 32;

    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    uint8_t alpha = static_cast<uint8_t>((100.0 - sprite->penData.color.transparency) / 100.0 * 255.0);

    const u32 color = C2D_Color32(rgbColor.r, rgbColor.g, rgbColor.b, alpha);
    const float thickness = std::clamp(static_cast<float>(sprite->penData.size * Render::renderScale), 1.0f, 1000.0f);

    const float xSscaled = (sprite->xPosition * Render::renderScale) + (getWidth() / 2);
    const float yScaled = (sprite->yPosition * -1 * Render::renderScale) + (getHeight() * 0.5f);

    fastPenQueue.push_back({FastPenType::DOT, xSscaled - (thickness / 2.0f), (yScaled - (thickness / 2.0f)) + PEN_Y_OFFSET, 0.0f, 0.0f, thickness, color});
}

void Renderer_C2D::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    uint8_t alpha = static_cast<uint8_t>((100.0 - sprite->penData.color.transparency) / 100.0 * 255.0);

    if (!hasFrameBegan) {
        if (!C3D_FrameBegin(C3D_FRAME_NONBLOCK)) C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        hasFrameBegan = true;
    }
    C2D_SceneBegin(penRenderTarget);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

    const int width = getWidth();
    const int height = getHeight();

    const int PEN_Y_OFFSET = Render::renderMode != Render::BOTH_SCREENS ? 16 : (screenHeight * 0.5) + 32;

    const float heightMultiplier = 0.5f;
    const u32 color = C2D_Color32(rgbColor.r, rgbColor.g, rgbColor.b, alpha);
    const float thickness = sprite->penData.size * Render::renderScale;

    const float x1_scaled = (x1 * Render::renderScale) + (width / 2);
    const float y1_scaled = (y1 * -1 * Render::renderScale) + (height * heightMultiplier) + PEN_Y_OFFSET;
    const float x2_scaled = (x2 * Render::renderScale) + (width / 2);
    const float y2_scaled = (y2 * -1 * Render::renderScale) + (height * heightMultiplier) + PEN_Y_OFFSET;

    C2D_DrawLine(x1_scaled, y1_scaled, color, x2_scaled, y2_scaled, color, thickness, 0);

    const float radius = thickness / 2.0f;
    C2D_DrawCircleSolid(x1_scaled, y1_scaled, 0, radius, color);
    C2D_DrawCircleSolid(x2_scaled, y2_scaled, 0, radius, color);
}

void Renderer_C2D::penDotAccurate(Sprite *sprite) {
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    if (!hasFrameBegan) {
        if (!C3D_FrameBegin(C3D_FRAME_NONBLOCK)) C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        hasFrameBegan = true;
    }
    C2D_SceneBegin(penRenderTarget);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

    const int PEN_Y_OFFSET = Render::renderMode != Render::BOTH_SCREENS ? 16 : (screenHeight * 0.5) + 32;

    const u32 color = C2D_Color32(rgbColor.r, rgbColor.g, rgbColor.b, 255);
    const int thickness = std::clamp(static_cast<int>(sprite->penData.size * Render::renderScale), 1, 1000);

    const float xSscaled = (sprite->xPosition * Render::renderScale) + (getWidth() / 2);
    const float yScaled = (sprite->yPosition * -1 * Render::renderScale) + (getHeight() * 0.5);
    const float radius = thickness / 2.0f;

    C2D_DrawCircleSolid(xSscaled, yScaled + PEN_Y_OFFSET, 0, radius, color);
}

void Renderer_C2D::penStamp(Sprite *sprite) {
    auto imgFind = Scratch::costumeImages.find(sprite->costumes[sprite->currentCostume].fullName);
    if (imgFind == Scratch::costumeImages.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return;
    }

    if (!fastPenQueue.empty()) {
        if (!hasFrameBegan) {
            if (!C3D_FrameBegin(C3D_FRAME_NONBLOCK)) C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            hasFrameBegan = true;
        }
        C2D_SceneBegin(penRenderTarget);
        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

        for (FastPenData &draw : fastPenQueue) {
            switch (draw.type) {
            case FastPenType::DOT:
                C2D_DrawRectSolid(draw.x1, draw.y1, 0.0f, draw.size, draw.size, draw.color);
                break;
            case FastPenType::LINE:
                C2D_DrawLine(draw.x1, draw.y1, draw.color, draw.x2, draw.y2, draw.color, draw.size, 0.0f);
                break;
            }
        }
        fastPenQueue.clear();
    }

    Image *image = imgFind->second.get();
    if (!hasFrameBegan) {
        if (!C3D_FrameBegin(C3D_FRAME_NONBLOCK)) C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        hasFrameBegan = true;
    }
    C2D_SceneBegin(penRenderTarget);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

    const bool isSVG = sprite->costumes[sprite->currentCostume].isSVG;
    Render::calculateRenderPosition(sprite, isSVG);
    const int PEN_Y_OFFSET = Render::renderMode != Render::BOTH_SCREENS ? 16 : (screenHeight * 0.5) + 32;

    ImageRenderParams params;
    params.centered = true;
    params.x = sprite->renderInfo.renderX;
    params.y = sprite->renderInfo.renderY + PEN_Y_OFFSET;
    params.rotation = sprite->renderInfo.renderRotation;
    params.scale = sprite->renderInfo.renderScaleY;
    params.flip = (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0);
    params.opacity = 1.0f - (std::clamp(sprite->ghostEffect, 0.0f, 100.0f) * 0.01f);
    params.brightness = sprite->brightnessEffect;
    image->render(params);
}

void Renderer_C2D::penClear() {
    if (penRenderTarget == nullptr || OS::toExit) return;
    if (!hasFrameBegan) {
        if (!C3D_FrameBegin(C3D_FRAME_NONBLOCK)) C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        hasFrameBegan = true;
    }
    C2D_TargetClear(penRenderTarget, C2D_Color32(0, 0, 0, 0));
}

void Renderer_C2D::cleanupPen() {
    if (penRenderTarget != nullptr) {
        C3D_RenderTargetDelete(penRenderTarget);
        C3D_TexDelete(penImage.tex);
        if (penTex != nullptr) {
            delete penTex;
            penTex = nullptr;
        }
        penRenderTarget = nullptr;
    }
}

void Renderer_C2D::beginFrame(int screen, int colorR, int colorG, int colorB) {
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
        currentScreen = 0;
        C2D_SceneBegin(topScreen);
    }
}

void Renderer_C2D::endFrame(bool shouldFlush) {
    C2D_Flush();
    C3D_FrameEnd(0);
    hasFrameBegan = false;
}

void Renderer_C2D::drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    C2D_DrawRectSolid(
        x - (w / 2.0f),
        y - (h / 2.0f),
        1,
        w,
        h,
        C2D_Color32(colorR, colorG, colorB, colorA));
}

void Renderer_C2D::drawRoundedBox(int w, int h, int x, int y, int radius, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    drawBox(w, h, x, y, colorR, colorG, colorB, colorA);
}

void Renderer_C2D::renderSprites() {
    if (isConsoleInit) Render::renderMode = Render::TOP_SCREEN_ONLY;
    if (!hasFrameBegan) {
        if (!C3D_FrameBegin(C3D_FRAME_NONBLOCK)) C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    }

    if (penRenderTarget && penRenderTarget != nullptr && !fastPenQueue.empty()) {
        C2D_SceneBegin(penRenderTarget);
        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

        for (FastPenData &draw : fastPenQueue) {
            switch (draw.type) {
            case FastPenType::DOT:
                C2D_DrawRectSolid(draw.x1, draw.y1, 0.0f, draw.size, draw.size, draw.color);
                break;
            case FastPenType::LINE:
                C2D_DrawLine(draw.x1, draw.y1, draw.color, draw.x2, draw.y2, draw.color, draw.size, 0.0f);
                break;
            }
        }
        fastPenQueue.clear();
    }

    float slider = osGet3DSliderState();
    const float depthScale = 8.0f / Scratch::sprites.size();

    const float SLIDER_THRESHOLD = 0.0f;
    bool use_slider = (is_2ds != 0) && (slider > SLIDER_THRESHOLD);

    const float image_offset_x = OBJ_OFFSET(40.0f);
    const float image_offset_y = OBJ_OFFSET(120.0f);

    const float speech_offset_x = OBJ_OFFSET(40.0f);
    const float speech_offset_y = OBJ_OFFSET(240.0f);

    auto screen_rendering = [&](bool eye_side, bool is_top_screen) {
        const bool dual = Render::renderMode == Render::BOTH_SCREENS && !is_top_screen;
        float pen_offset_x = dual ? -40.0f : 0.0f;
        float pen_offset_y = dual ? -240.0f : 0.0f;

        size_t i = 0;
        for (auto it = Scratch::sprites.rbegin(); it != Scratch::sprites.rend(); ++it) {
            Sprite *currentSprite = *it;

            if (i == 1 && penRenderTarget != nullptr) {
                C2D_DrawImageAt(penImage, pen_offset_x, pen_offset_y, 0, nullptr, 1.0f, 1.0f);
            }

            if (!currentSprite->visible) continue;

            int costumeIndex = 0;
            for (const auto &costume : currentSprite->costumes) {
                if (costumeIndex == currentSprite->currentCostume) {
                    if (!is_top_screen) {
                        renderImage(currentSprite, costume.fullName, true, image_offset_x, image_offset_y);
                        break;
                    }

                    size_t totalSprites = Scratch::sprites.size();
                    float eyeOffset = slider * (static_cast<float>(totalSprites - 1 - i) * depthScale);

                    if (eye_side == 0) {
                        eyeOffset *= -1;
                    }

                    renderImage(currentSprite, costume.fullName, false, (use_slider) ? eyeOffset : 0.0f, OBJ_OFFSET((-120)));
                    break;
                }
                costumeIndex++;
            }
            i++;
        }
        if (speechManager) {
            speechManager->render(speech_offset_x, speech_offset_y);
        }
    };

    if (Render::renderMode != Render::BOTTOM_SCREEN_ONLY) {
        C2D_SceneBegin(topScreen);
        C2D_TargetClear(topScreen, clrWhite);
        currentScreen = 0;

        screen_rendering(0, true);

        Render::renderMonitors();
        if (Render::renderMode != Render::BOTH_SCREENS) {
            drawBlackBars(screenWidth, screenHeight);
        }

        if (Input::mousePointer.isMoving) {
            C2D_DrawRectSolid((Input::mousePointer.x * Render::renderScale) + (screenWidth * 0.5f),
                              (Input::mousePointer.y * -1 * Render::renderScale) + (screenHeight * 0.5f), 1, 5, 5, clrGreen);
            Input::mousePointer.x = std::clamp((float)Input::mousePointer.x, -Scratch::projectWidth * 0.5f, Scratch::projectWidth * 0.5f);
            Input::mousePointer.y = std::clamp((float)Input::mousePointer.y, -Scratch::projectHeight * 0.5f, Scratch::projectHeight * 0.5f);
        }

        if (use_slider) {
            C2D_SceneBegin(topScreenRightEye);
            C2D_TargetClear(topScreenRightEye, clrWhite);

            screen_rendering(1, true);

            Render::renderMonitors();
            if (Render::renderMode != Render::BOTH_SCREENS) {
                drawBlackBars(screenWidth, screenHeight);
            }
        }
    }

    if (Render::renderMode == Render::BOTH_SCREENS || Render::renderMode == Render::BOTTOM_SCREEN_ONLY) {
        C2D_SceneBegin(bottomScreen);
        C2D_TargetClear(bottomScreen, clrWhite);

        if (Render::renderMode != Render::BOTH_SCREENS)
            currentScreen = 1;

        screen_rendering(0, false);

        if (Render::renderMode == Render::BOTH_SCREENS) {
            Render::renderMonitors(-40, -240);
        } else {
            drawBlackBars(bottomScreenWidth, screenHeight);
            Render::renderMonitors();
        }
    }

    C3D_FrameEnd(0);
    C2D_Flush();
    osSetSpeedupEnable(true);
    hasFrameBegan = false;
}

void Renderer_C2D::renderPenLayer() {
    // unused as its handled in renderSprites
}