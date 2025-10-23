#pragma once
#include "interpret.hpp"
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
    static bool sizeChanged;

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
    static void calculateRenderPosition(Sprite *sprite, const bool &isSVG) {
        const int screenWidth = getWidth();
        const int screenHeight = getHeight();

        // If the window size changed, or if the sprite changed costumes
        if (sizeChanged || sprite->currentCostume != sprite->renderInfo.oldCostumeID) {
            // change all renderinfo a bit to update position for all
            sprite->renderInfo.oldX++;
            sprite->renderInfo.oldY++;
            sprite->renderInfo.oldRotation++;
            sprite->renderInfo.oldSize++;
            sprite->renderInfo.oldCostumeID = sprite->currentCostume;
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

            int renderX;
            int renderY;
            int spriteX = static_cast<int>(sprite->xPosition);
            int spriteY = static_cast<int>(sprite->yPosition);

            // Handle if the sprite's image is not centered in the costume editor
            if (sprite->spriteWidth - sprite->rotationCenterX != 0 ||
                sprite->spriteHeight - sprite->rotationCenterY != 0) {

                const int offsetX = (sprite->spriteWidth - sprite->rotationCenterX) >> (!isSVG ? 1 : 0);
                const int offsetY = (sprite->spriteHeight - sprite->rotationCenterY) >> (!isSVG ? 1 : 0);

                // Offset based on size
                if (sprite->size != 100.0f) {
                    const float scale = sprite->size * (isSVG ? 0.01 : 0.005);
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

            if (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0) {
                spriteX += sprite->spriteWidth * (isSVG ? 2 : 1);
                spriteX *= -1;
            }

            if (renderMode != BOTH_SCREENS && (screenWidth != Scratch::projectWidth || screenHeight != Scratch::projectHeight)) {
                renderX = (spriteX * renderScale) + (screenWidth >> 1);
                renderY = (-spriteY * renderScale) + (screenHeight >> 1);
            } else {
                renderX = spriteX + (screenWidth >> 1);
                renderY = -spriteY + (screenHeight >> 1);
            }

#ifdef SDL_BUILD
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
        sizeChanged = true;
    }

    /**
     * Renders all visible variable and list monitors
     */
    static void renderVisibleVariables();

    /**
     * Renders the pen layer
     */
    static void renderPenLayer();

    /**
     * Draws a simple box to the screen.
     */
    static void drawBox(int w, int h, int x, int y, uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA);

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
