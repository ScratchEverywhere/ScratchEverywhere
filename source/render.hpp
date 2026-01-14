#pragma once
#include <chrono>
#include <math.hpp>
#include <runtime.hpp>
#include <sprite.hpp>
#include <text.hpp>
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
    static void calculateRenderPosition(Sprite *sprite, const bool &isSVG) {
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
            sprite->renderInfo.oldRotation++;
            sprite->renderInfo.oldX++;
            sprite->renderInfo.oldY++;
#ifdef RENDERER_GL2D
            sprite->renderInfo.renderScaleX = sprite->size * 0.005;
#else
            sprite->renderInfo.renderScaleX = sprite->size * (isSVG ? 0.01 : 0.005);
#endif

            if (renderMode != BOTH_SCREENS && screenHeight != Scratch::projectHeight) {
                float scale = std::min(static_cast<float>(screenWidth) / Scratch::projectWidth,
                                       static_cast<float>(screenHeight) / Scratch::projectHeight);
                sprite->renderInfo.renderScaleX *= scale;
            }
            sprite->renderInfo.renderScaleY = sprite->renderInfo.renderScaleX;
        }
        if (sprite->rotation != sprite->renderInfo.oldRotation) {
            sprite->renderInfo.oldRotation = sprite->rotation;
            sprite->renderInfo.oldSize++;
            sprite->renderInfo.oldX++;
            sprite->renderInfo.oldY++;
            if (sprite->rotationStyle == sprite->ALL_AROUND) {
                sprite->renderInfo.renderRotation = Math::degreesToRadians(sprite->rotation - 90);
            } else {
                sprite->renderInfo.renderRotation = 0;
            }
        }
        if (sprite->xPosition != sprite->renderInfo.oldX ||
            sprite->yPosition != sprite->renderInfo.oldY) {

            sprite->renderInfo.oldX = sprite->xPosition;
            sprite->renderInfo.oldY = sprite->yPosition;

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

#ifdef RENDERER_CITRO2D
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

#if defined(RENDERER_SDL1) || defined(RENDERER_SDL2) || defined(RENDERER_SDL3)
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
        for (auto &sprite : Scratch::sprites) {
            sprite->renderInfo.forceUpdate = true;
        }
    }

    /**
     * Gets the color for a monitor value background based on opcode
     */
    static ColorRGBA getMonitorValueColor(const std::string &opcode) {
        if (opcode.substr(0, 5) == "data_")
            return {.r = 255, .g = 140, .b = 26, .a = 255};
        else if (opcode.substr(0, 8) == "sensing_")
            return {.r = 92, .g = 177, .b = 214, .a = 255};
        else if (opcode.substr(0, 7) == "motion_")
            return {.r = 76, .g = 151, .b = 255, .a = 255};
        else if (opcode.substr(0, 6) == "looks_")
            return {.r = 153, .g = 102, .b = 255, .a = 255};
        else if (opcode.substr(0, 6) == "sound_")
            return {.r = 207, .g = 99, .b = 207, .a = 255};
        else return {.r = 255, .g = 140, .b = 26, .a = 255};
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

        // FIXME: the text is slightly lower on OpenGL
        for (auto &var : visibleVariables) {
            if (var.visible) {

                // Weird Turbowarp math for monitor positions on custom sized projects
                float projectX = var.x + (Scratch::projectWidth - 480) * 0.5f;
                float projectY = var.y + (Scratch::projectHeight - 360) * 0.5f;

                if (var.mode == "list") {
                    if (listMonitors.find(var.id) == listMonitors.end()) {
                        ListMonitorRenderObjects newObj;
                        newObj.name = createTextObject(var.displayName, 0, 0);
                        newObj.length = createTextObject("", 0, 0);
                        listMonitors[var.id] = std::move(newObj);
                    }
                    ListMonitorRenderObjects &monitorGfx = listMonitors[var.id];
                    monitorGfx.name->setText(var.displayName);
                    monitorGfx.name->setCenterAligned(true);
                    monitorGfx.name->setScale(1.0f * (scale / 2.0f));
                    monitorGfx.name->setColor(Math::color(0, 0, 0, 255));

                    float monitorX = (projectX * scale + barOffsetX) + (4 * scale);
                    float monitorY = (projectY * scale + barOffsetY) + (2 * scale);

                    const float boxHeight = monitorGfx.name->getSize()[1] + (2 * scale);

                    float monitorW = var.width * scale;
                    float monitorH = std::max(static_cast<float>(var.height * scale), (boxHeight * 2 + (8 * scale)) + var.list.size() * (boxHeight + 2 * scale));

                    // Draw background
                    drawBox(monitorW + (2 * scale), monitorH + (2 * scale), monitorX + (monitorW / 2), monitorY + (monitorH / 2), 194, 204, 217);
                    drawBox(monitorW, monitorH, monitorX + (monitorW / 2), monitorY + (monitorH / 2), 229, 240, 255);

                    // List name background
                    drawBox(monitorW, boxHeight, monitorX + monitorW / 2, monitorY + (boxHeight / 2), 255, 255, 255);

                    // List name text
                    monitorGfx.name->render(monitorX + (monitorW / 2), monitorY + (4 * scale) + (monitorGfx.name->getSize()[1] / 2));

                    // Items
                    if (monitorGfx.items.size() != var.list.size()) {
                        monitorGfx.items.clear();
                        monitorGfx.indices.clear();

                        monitorGfx.items.reserve(var.list.size());
                        monitorGfx.indices.reserve(var.list.size());
                        for (size_t i = 0; i < var.list.size(); ++i) {
                            monitorGfx.items.push_back(createTextObject("", 0, 0));
                            monitorGfx.indices.push_back(createTextObject("", 0, 0));
                        }
                    }

                    if (!var.list.empty()) {
                        float item_y = 4 * scale;
                        int index = 1;

                        for (const auto &s : var.list) {
                            drawBox(monitorW - (24 * scale), boxHeight, monitorX + (22 * scale) + (monitorW - (28 * scale)) / 2, monitorY + boxHeight + item_y + (boxHeight / 2), 252, 102, 44);

                            std::unique_ptr<TextObject> &itemText = monitorGfx.items[index - 1];
                            itemText->setText(s.asString());
                            itemText->setColor(Math::color(255, 255, 255, 255));
                            itemText->setScale(1.0f * (scale / 2.0f));
                            itemText->setCenterAligned(false);

                            std::unique_ptr<TextObject> &itemIndexText = monitorGfx.indices[index - 1];
                            itemIndexText->setText(std::to_string(index));
                            itemIndexText->setColor(Math::color(0, 0, 0, 255));
                            itemIndexText->setScale(1.0f * (scale / 2.0f));
                            itemIndexText->setCenterAligned(true);

                            itemText->render(monitorX + (24 * scale), monitorY + boxHeight + (4 * scale) + item_y);
                            itemIndexText->render(monitorX + (10 * scale), monitorY + boxHeight + (12 * scale) + item_y);

                            index++;
                            item_y += boxHeight + (4 * scale);
                        }
                    } else {
                        std::unique_ptr<TextObject> empty = createTextObject("(empty)", 0, 0);
                        empty->setColor(Math::color(0, 0, 0, 255));
                        empty->setScale(1.0f * (scale / 2.0f));
                        empty->setCenterAligned(true);
                        empty->render(monitorX + (monitorW / 2), monitorY + boxHeight + (12 * scale));
                    }

                    // list length background
                    drawBox(monitorW, boxHeight, monitorX + (monitorW / 2), monitorY + monitorH - (boxHeight / 2), 255, 255, 255);

                    // list length text
                    monitorGfx.length->setText("length " + std::to_string(var.list.size()));
                    monitorGfx.length->setCenterAligned(true);
                    monitorGfx.length->setScale(1.0f * (scale / 2.0f));
                    monitorGfx.length->setColor(Math::color(0, 0, 0, 255));
                    monitorGfx.length->render(monitorX + (monitorW / 2), monitorY + monitorH - (6 * scale));

                    // what the hell, sure
                    std::unique_ptr<TextObject> plus = createTextObject("+", 0, 0);
                    plus->setColor(Math::color(0, 0, 0, 255));
                    plus->setScale(1.0f * (scale / 2.0f));
                    plus->render(monitorX + (8 * scale), monitorY + monitorH - (6 * scale));

                    std::unique_ptr<TextObject> equal = createTextObject("=", 0, 0);
                    equal->setColor(Math::color(0, 0, 0, 255));
                    equal->setScale(1.0f * (scale / 2.0f));
                    equal->render(monitorX + monitorW - (8 * scale), monitorY + monitorH - (6 * scale));
                } else {
                    std::string renderText = var.value.asString();
                    if (monitorTexts.find(var.id) == monitorTexts.end()) {
                        monitorTexts[var.id].first = createTextObject(var.displayName.empty() ? " " : var.displayName, var.x, var.y);
                        monitorTexts[var.id].second = createTextObject(renderText.empty() ? " " : renderText, var.x, var.y);
                    } else {
                        monitorTexts[var.id].first->setText(var.displayName);
                        monitorTexts[var.id].second->setText(renderText);
                    }

                    std::unique_ptr<TextObject> &nameObj = monitorTexts[var.id].first;
                    std::unique_ptr<TextObject> &valueObj = monitorTexts[var.id].second;

                    const std::vector<float> nameSizeBox = nameObj->getSize();
                    const std::vector<float> valueSizeBox = valueObj->getSize();

                    // Get color based on opcode
                    ColorRGBA valueBackgroundColor = getMonitorValueColor(var.opcode);

                    nameObj->setCenterAligned(false);
                    valueObj->setCenterAligned(false);

                    float baseRenderX = projectX * scale + barOffsetX;
                    float baseRenderY = projectY * scale + barOffsetY;

                    if (var.mode == "large") {
                        valueObj->setColor(Math::color(255, 255, 255, 255));
                        valueObj->setScale(1.25f * (scale / 2.0f));

                        float valueWidth = std::max(40 * scale, valueSizeBox[0] + (4 * scale));

                        // Draw value background
                        drawBox(valueWidth + (2 * scale), valueSizeBox[1] + (2 * scale),
                                baseRenderX + valueWidth / 2, baseRenderY + valueSizeBox[1] / 2,
                                194, 204, 217);
                        drawBox(valueWidth, valueSizeBox[1],
                                baseRenderX + valueWidth / 2, baseRenderY + valueSizeBox[1] / 2,
                                valueBackgroundColor.r, valueBackgroundColor.g, valueBackgroundColor.b);

                        float valueCenterX = baseRenderX + (valueWidth / 2) - (valueSizeBox[0] / 2);
                        valueObj->render(valueCenterX, baseRenderY + (3 * scale));
                    } else {
                        nameObj->setColor(Math::color(0, 0, 0, 255));
                        nameObj->setScale(1.0f * (scale / 2.0f));
                        valueObj->setColor(Math::color(255, 255, 255, 255));
                        valueObj->setScale(1.0f * (scale / 2.0f));

                        float monitorWidth = 8 * scale;
                        float valueWidth = std::max(40 * scale, valueSizeBox[0] + (8 * scale));

                        // Draw name background
                        float nameBackgroundX = baseRenderX + monitorWidth;
                        float nameBackgroundY = baseRenderY + 4 * scale;
                        float nameBackgroundWidth = nameSizeBox[0] + valueWidth;
                        float nameBackgroundHeight = std::max(nameSizeBox[1], valueSizeBox[1]);
                        drawBox(nameBackgroundWidth + (14 * scale), nameBackgroundHeight + (6 * scale),
                                nameBackgroundX + 2 + nameBackgroundWidth / 2, nameBackgroundY + nameBackgroundHeight / 2,
                                194, 204, 217);
                        drawBox(nameBackgroundWidth + (12 * scale), nameBackgroundHeight + (4 * scale),
                                nameBackgroundX + 2 + nameBackgroundWidth / 2, nameBackgroundY + nameBackgroundHeight / 2,
                                229, 240, 255);

                        monitorWidth += nameSizeBox[0] + (4 * scale);

                        // Draw value background
                        float valueBackgroundX = baseRenderX + monitorWidth;
                        float valueBackgroundY = baseRenderY + 4 * scale;
                        drawBox(valueWidth, valueSizeBox[1],
                                valueBackgroundX + valueWidth / 2, valueBackgroundY + valueSizeBox[1] / 2,
                                valueBackgroundColor.r, valueBackgroundColor.g, valueBackgroundColor.b);

                        nameObj->render(nameBackgroundX, nameBackgroundY + (2 * scale));
                        valueObj->render(valueBackgroundX + (valueWidth / 2) - (valueSizeBox[0] / 2), valueBackgroundY + (2 * scale));
                    }
                }
            } else {
                if (monitorTexts.find(var.id) != monitorTexts.end()) {
                    monitorTexts.erase(var.id);
                }
                if (listMonitors.find(var.id) != listMonitors.end()) {
                    auto &monitorGfx = listMonitors[var.id];
                    listMonitors.erase(var.id);
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
    static std::unordered_map<std::string, std::pair<std::unique_ptr<TextObject>, std::unique_ptr<TextObject>>> monitorTexts;

    struct ListMonitorRenderObjects {
        std::unique_ptr<TextObject> name;
        std::unique_ptr<TextObject> length;
        std::vector<std::unique_ptr<TextObject>> items;
        std::vector<std::unique_ptr<TextObject>> indices;
    };
    static std::unordered_map<std::string, ListMonitorRenderObjects> listMonitors;

    static std::vector<Monitor> visibleVariables;
};
