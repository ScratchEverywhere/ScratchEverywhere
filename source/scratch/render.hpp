#pragma once
#include "interpret.hpp"
#include "math.hpp"
#include "sprite.hpp"
#include "text.hpp"
#include <chrono>
#include <vector>

class Render {
  public:
    static std::chrono::system_clock::time_point startTime;
    static std::chrono::system_clock::time_point endTime;
    static bool debugMode;
    static float renderScale;

    static bool hasFrameBegan;

    static bool Init();

    static bool initPen();

    static void deInit();

    /**
     * [SDL] returns the current renderer.
     */
    static void *getRenderer();

    /**
     * Begins a drawing frame.
     * @param screen [3DS] which screen to begin drawing on. 0 = top, 1 = bottom.
     * @param colorR red 0-255
     * @param colorG green 0-255
     * @param colorB blue 0-255
     */
    static void beginFrame(int screen, int colorR, int colorG, int colorB);

    /**
     * Stops drawing.
     * @param shouldFlush determines whether or not images can get freed as the frame ends.
     */
    static void endFrame(bool shouldFlush = true);

    /**
     * gets the screen Width.
     */
    static int getWidth();
    /**
     * gets the screen Height.
     */
    static int getHeight();

    /**
     * Renders every sprite to the screen.
     */
    static void renderSprites();

    /**
     * Fills a sprite's `renderInfo` with information on where to render on screen.
     * @param sprite the sprite to calculate.
     * @param isSVG if the sprite's current costume is a Vector image.
     */
    static void calculateRenderPosition(Sprite *sprite, bool isSVG) {
        const int screenWidth = getWidth();
        const int screenHeight = getHeight();

        // If the window size changed, or if the sprite changed costumes
        if (sprite->renderInfo.forceUpdate || sprite->currentCostume != sprite->renderInfo.oldCostumeID) {
            // change all renderinfo a bit to update position for all
            sprite->renderInfo.oldX++;
            sprite->renderInfo.oldY++;
            sprite->renderInfo.oldRotation++;
            sprite->renderInfo.oldSize++;
            sprite->renderInfo.oldCostumeID = sprite->currentCostume;
            sprite->renderInfo.forceUpdate = false;
        }

        if (sprite->size != sprite->renderInfo.oldSize) {
            sprite->renderInfo.oldSize = sprite->size;
            sprite->renderInfo.oldX++;
            sprite->renderInfo.oldY++;
            sprite->renderInfo.renderScaleX = sprite->size * (isSVG ? 0.01 : 0.005);
            if (renderMode != BOTH_SCREENS && screenHeight != Scratch::projectHeight) {
                float scale = std::min(static_cast<float>(screenWidth) / Scratch::projectWidth,
                                       static_cast<float>(screenHeight) / Scratch::projectHeight);
                sprite->renderInfo.renderScaleX *= scale;
            }
            sprite->renderInfo.renderScaleY = sprite->renderInfo.renderScaleX;
        }
        if (sprite->rotation != sprite->renderInfo.oldRotation) {
            sprite->renderInfo.oldRotation = sprite->rotation;
            sprite->renderInfo.oldX++;
            sprite->renderInfo.oldY++;
            if (sprite->rotationStyle == sprite->ALL_AROUND) {
                sprite->renderInfo.renderRotation = Math::degreesToRadians(sprite->rotation - 90);
            } else {
                sprite->renderInfo.renderRotation = 0;
            }
            if (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0) {
                sprite->renderInfo.renderScaleX = -std::abs(sprite->renderInfo.renderScaleX);
            }
        }
        if (sprite->xPosition != sprite->renderInfo.oldX ||
            sprite->yPosition != sprite->renderInfo.oldY) {

            sprite->renderInfo.oldX = sprite->xPosition;
            sprite->renderInfo.oldY = sprite->yPosition;

#ifdef __NDS__
            isSVG = true;
#endif

            float renderX;
            float renderY;
            float spriteX = static_cast<int>(sprite->xPosition);
            float spriteY = static_cast<int>(sprite->yPosition);

            // Handle if the sprite's image is not centered in the costume editor
            if (sprite->spriteWidth - sprite->rotationCenterX != 0 ||
                sprite->spriteHeight - sprite->rotationCenterY != 0) {
                const int shiftAmount = !isSVG ? 1 : 0;
                int offsetX = (sprite->spriteWidth - sprite->rotationCenterX) >> shiftAmount;
                const int offsetY = (sprite->spriteHeight - sprite->rotationCenterY) >> shiftAmount;

                if (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0)
                    offsetX *= -1;

                // Offset based on size
                if (sprite->size != 100.0f) {
                    const float scale = sprite->size * 0.01;
                    const float scaledX = offsetX * scale;
                    const float scaledY = offsetY * scale;

                    spriteX += scaledX - offsetX;
                    spriteY -= scaledY - offsetY;
                }

                // Offset based on rotation
                if (sprite->renderInfo.renderRotation != 0) {
                    float rot = sprite->renderInfo.renderRotation;
                    float rotatedX = -offsetX * std::cos(rot) + offsetY * std::sin(rot);
                    float rotatedY = -offsetX * std::sin(rot) - offsetY * std::cos(rot);
                    spriteX += rotatedX;
                    spriteY -= rotatedY;
                } else {
                    spriteX += offsetX;
                    spriteY -= offsetY;
                }
            }

#ifndef __NDS__
            if (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0) {

                spriteX -= sprite->spriteWidth * (isSVG ? 2 : 1);
            }
#endif

            if (renderMode != BOTH_SCREENS && (screenWidth != Scratch::projectWidth || screenHeight != Scratch::projectHeight)) {
                renderX = (spriteX * renderScale) + (screenWidth >> 1);
                renderY = (-spriteY * renderScale) + (screenHeight >> 1);
            } else {
                renderX = static_cast<int>(spriteX + (screenWidth >> 1));
                renderY = static_cast<int>(-spriteY + (screenHeight >> 1));
            }

#if defined(RENDERER_SDL2) || defined(RENDERER_SDL3)
            renderX -= (sprite->spriteWidth * sprite->renderInfo.renderScaleY);
            renderY -= (sprite->spriteHeight * sprite->renderInfo.renderScaleY);
#endif

            sprite->renderInfo.renderX = renderX;
            sprite->renderInfo.renderY = renderY;
        }
    }

    /**
     * Sets the sprite rendering scale, based on the aspect ratio of the project and the window's dimension.
     * This should be called every time either the project or the window changes resolution.
     */
    static void setRenderScale() {
        const int screenWidth = getWidth();
        const int screenHeight = getHeight();
        renderScale = std::min(static_cast<float>(screenWidth) / Scratch::projectWidth,
                               static_cast<float>(screenHeight) / Scratch::projectHeight);
        if (renderMode == BOTH_SCREENS) renderScale = 1.0f;
        forceUpdateSpritePosition();
    }

    /**
     * Force updates every sprite's position on screen. Should be called when window size changes.
     */
    static void forceUpdateSpritePosition() {
        for (auto &sprite : sprites) {
            sprite->renderInfo.forceUpdate = true;
        }
    }

    /**
     * Renders all visible variable and list monitors
     */
    static void renderVisibleVariables() {
        // get screen scale
        const float scale = renderScale;
        const float screenWidth = getWidth();
        const float screenHeight = getHeight();

        // calculate black bar offset
        float screenAspect = static_cast<float>(screenWidth) / screenHeight;
        float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;
        float barOffsetX = 0.0f;
        float barOffsetY = 0.0f;
        if (screenAspect > projectAspect) {
            float scaledProjectWidth = Scratch::projectWidth * scale;
            barOffsetX = (screenWidth - scaledProjectWidth) / 2.0f;
        } else if (screenAspect < projectAspect) {
            float scaledProjectHeight = Scratch::projectHeight * scale;
            barOffsetY = (screenHeight - scaledProjectHeight) / 2.0f;
        }

        for (auto &var : visibleVariables) {
            if (var.visible) {
                std::string renderText = BlockExecutor::getMonitorValue(var).asString();
                if (monitorTexts.find(var.id) == monitorTexts.end()) {
                    monitorTexts[var.id] = createTextObject(renderText, var.x, var.y);
                } else {
                    monitorTexts[var.id]->setText(renderText);
                }
                float renderX = var.x * scale + barOffsetX;
                float renderY = var.y * scale + barOffsetY;
                const std::vector<float> renderSize = monitorTexts[var.id]->getSize();
                ColorRGBA backgroundColor = {
                    .r = 228,
                    .g = 240,
                    .b = 255,
                    .a = 255};

                monitorTexts[var.id]->setCenterAligned(false);
                if (var.mode != "large") {
                    monitorTexts[var.id]->setColor(Math::color(0, 0, 0, 255));
                    monitorTexts[var.id]->setScale(1.0f * (scale / 2.0f));
                } else {
                    monitorTexts[var.id]->setColor(Math::color(255, 255, 255, 255));
                    monitorTexts[var.id]->setScale(1.25f * (scale / 2.0f));
                    backgroundColor = {
                        .r = 255,
                        .g = 141,
                        .b = 41,
                        .a = 255};
                }

                // draw background
                drawBox(renderSize[0] + (2 * scale), renderSize[1] + (2 * scale), renderX + renderSize[0] / 2, renderY + renderSize[1] / 2, 194, 204, 217);
                drawBox(renderSize[0], renderSize[1], renderX + renderSize[0] / 2, renderY + renderSize[1] / 2, backgroundColor.r, backgroundColor.g, backgroundColor.b);
#ifdef __3DS__
                renderY += renderSize[1] / 2;
#endif
                monitorTexts[var.id]->render(renderX, renderY);
            } else {
                if (monitorTexts.find(var.id) != monitorTexts.end()) {
                    delete monitorTexts[var.id];
                    monitorTexts.erase(var.id);
                }
            }
        }
    }

    /**
     * Renders the pen layer
     */
    static void renderPenLayer();

    /**
     * Draws a simple box to the screen.
     */
    static void drawBox(int w, int h, int x, int y, uint8_t colorR = 0, uint8_t colorG = 0, uint8_t colorB = 0, uint8_t colorA = 255);

    /**
     * Returns whether or not the app should be running.
     * If `false`, the app should close.
     */
    static bool appShouldRun();

    /**
     * Called whenever the pen is down and a sprite moves (so a line should be drawn.)
     */
    static void penMove(double x1, double y1, double x2, double y2, Sprite *sprite);

    /**
     * Called on pen down to place a singular dot at the position of the sprite.
     */
    static void penDot(Sprite *sprite);

    /**
     * Called whenever the stamp block is used to place a copy of the sprite onto the pen canvas.
     */
    static void penStamp(Sprite *sprite);

    /**
     * Called when the pen canvas needs to be cleared.
     */
    static void penClear();

    /**
     * Returns whether or not enough time has passed to advance a frame.
     * @return True if we should go to the next frame, False otherwise.
     */
    static bool checkFramerate() {
        if (Scratch::turbo) return true;
        static Timer frameTimer;
        int frameDuration = 1000 / Scratch::FPS;
        return frameTimer.hasElapsedAndRestart(frameDuration);
    }

    enum RenderModes {
        TOP_SCREEN_ONLY,
        BOTTOM_SCREEN_ONLY,
        BOTH_SCREENS
    };

    static RenderModes renderMode;
    static std::unordered_map<std::string, TextObject *> monitorTexts;

    static std::vector<Monitor> visibleVariables;
};
