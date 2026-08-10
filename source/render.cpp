#include "render.hpp"
#include <cstdint>
#include <log.hpp>

#include "blueprint.hpp"
#include "data/entity_components.hpp"
#include "runtime/data/monitor.hpp"
#include "runtime/entity_manager.hpp"
#include "runtime/systems/costume_system.hpp"
#include "runtime/vm/engine_state.hpp"
#include "vm/engine_state.hpp"

std::unordered_map<uint16_t, std::pair<std::unique_ptr<TextObject>, std::unique_ptr<TextObject>>> Render::monitorTexts;
std::unordered_map<uint16_t, Render::ListMonitorRenderObjects> Render::listMonitors;
bool Render::debugMode = false;
float Render::renderScale = 1.0f;
Render::RenderModes Render::renderMode = Render::TOP_SCREEN_ONLY;
bool Render::hasFrameBegan;
std::vector<Monitor> Render::monitors;

void Render::calculateRenderPosition(const uint32_t spriteID ) {
    RenderInfo &info = EntityManager::renderInfo[spriteID];
    const uint8_t spatialDirtyMask = DIRTY_POSITION | DIRTY_ROTATION | DIRTY_SIZE;
    if ((info.dirtyFlags & spatialDirtyMask) == 0) {
        return;
    }

    const int screenWidth = getWidth();
    const int screenHeight = getHeight();

    const SpriteTransform &transform = EntityManager::transforms[spriteID];
    const TargetDefinition &blueprint = EntityManager::blueprints[EntityManager::blueprintIds[spriteID]];
    const Costume &costume = blueprint.costumes[info.costumeId];

    if (info.isSizeDirty()) {
        info.scaleX = (transform.size * 0.01f) / costume.bitmapResolution;

        if (renderMode != BOTH_SCREENS && screenHeight != EngineState::projectHeight) {
            float scale = std::min(
                static_cast<float>(screenWidth) / EngineState::projectWidth,
                static_cast<float>(screenHeight) / EngineState::projectHeight);
            info.scaleX *= scale;
        }

        if (EngineState::bitmapHalfQuality && !costume.isSVG && costume.bitmapResolution == 2) {
            info.scaleX *= 2.0f;
        }

        info.scaleY = info.scaleX;

        info.makePositionDirty();
    }

    if (info.isRotationDirty()) {
        if (transform.rotationStyle == RotationStyle::ALL_AROUND) {
            info.rotation = Math::degreesToRadians(transform.direction - 90.0f);
        } else {
            info.rotation = 0.0f;
        }

        info.makePositionDirty();
    }

    if (info.isPositionDirty()) {
        float spriteX = transform.x;
        float spriteY = transform.y;

        const float rotCenterX = static_cast<float>(costume.rotationCenterX * 2.0);
        const float rotCenterY = static_cast<float>(costume.rotationCenterY * 2.0);

        if (transform.width - rotCenterX != 0.0f || transform.height - rotCenterY != 0.0f) {
            float offsetX = (transform.width - rotCenterX) * 0.5f;
            float offsetY = (transform.height - rotCenterY) * 0.5f;

            if (transform.rotationStyle == RotationStyle::LEFT_RIGHT && transform.direction < 0.0f) {
                offsetX *= -1.0f;
            }

            const float scale = (transform.size * 0.01f) / costume.bitmapResolution;
            offsetX *= scale;
            offsetY *= scale;

            if (info.rotation != 0.0f) {
                const float rotCos = std::cos(info.rotation);
                const float rotSin = std::sin(info.rotation);

                const float rotX = offsetX * rotCos - offsetY * rotSin;
                const float rotY = offsetX * rotSin + offsetY * rotCos;

                offsetX = rotX;
                offsetY = rotY;
            }

            spriteX += offsetX;
            spriteY -= offsetY;
        }

        if (renderMode != BOTH_SCREENS && (screenWidth != EngineState::projectWidth || screenHeight != EngineState::projectHeight)) {
            info.x = (spriteX * renderScale) + (screenWidth * 0.5f);
            info.y = (-spriteY * renderScale) + (screenHeight * 0.5f);
        } else {
            info.x = spriteX + (screenWidth * 0.5f);
            info.y = -spriteY + (screenHeight * 0.5f);
        }
    }

    info.clearDirtyFlags();
}

void Render::setRenderScale() {
    const int screenWidth = getWidth();
    const int screenHeight = getHeight();
    renderScale = std::min(static_cast<float>(screenWidth) / EngineState::projectWidth,
                           static_cast<float>(screenHeight) / EngineState::projectHeight);
    if (renderMode == BOTH_SCREENS) renderScale = 1.0f;
    forceUpdateSpritePosition();
}

void Render::resizeSVGs() {
    for (uint32_t spriteID : EntityManager::renderOrder) {
        resizeSVGs(spriteID);
    }
}

void Render::resizeSVGs(uint32_t spriteID) {
    SpriteTransform &transform = EntityManager::transforms[spriteID];
    TargetDefinition &def = EntityManager::blueprints[EntityManager::blueprintIds[spriteID]];

    const int screenWidth = getWidth();
    const int screenHeight = getHeight();

    for (auto &costume : def.costumes) {
        auto imgFind = CostumeSystem::costumeImages.find(costume.fullName);
        if (imgFind == CostumeSystem::costumeImages.end()) continue;

        float scale = transform.size / 100;
        scale *= std::min(static_cast<float>(screenWidth) / EngineState::projectWidth, static_cast<float>(screenHeight) / EngineState::projectHeight);

        auto potentialError = imgFind->second->resizeSVG(scale);
        if (!potentialError.has_value()) Log::logWarning("Error resizing SVG: " + costume.id);
    }
}

void Render::forceUpdateSpritePosition() {
    for (RenderInfo &render : EntityManager::renderInfo) {
        render.makePositionDirty();
        render.makeSizeDirty();
        render.makeRotationDirty();
    }
}

bool Render::checkFramerate() {
    static Timer frameTimer;
    int frameDuration = 1000 / EngineState::fps;
    return frameTimer.hasElapsedAndRestart(frameDuration);
}

std::string Render::getVariableValueString(Value value) {
    if (value.isDouble()) {
        return Math::toString(std::round(value.asDouble() * 1e6) / 1e6); // js Number(value.toFixed(6))
    } else if (value.isUndefined()) {
        return ""; // Scratch keeps the original value, leave blank for now
    } else {
        return value.asString();
    }
}

std::string Render::getListValueString(Value value) {
    if (value.isUndefined()) {
        return ""; // Scratch crashes, TurboWarp shows empty string
    } else {
        return value.asString();
    }
}

void Render::renderMonitors(const int &offsetX, const int &offsetY) {
    // get screen scale
    const float scale = renderScale;
    const float screenWidth = getWidth();
    const float screenHeight = getHeight();

    // calculate black bar offset
    float screenAspect = static_cast<float>(screenWidth) / screenHeight;
    float projectAspect = static_cast<float>(EngineState::projectWidth) / EngineState::projectHeight;
    float barOffsetX = 0.0f;
    float barOffsetY = 0.0f;
    if (screenAspect > projectAspect) {
        float scaledProjectWidth = EngineState::projectWidth * scale;
        barOffsetX = (screenWidth - scaledProjectWidth) / 2.0f;
    } else if (screenAspect < projectAspect) {
        float scaledProjectHeight = EngineState::projectHeight * scale;
        barOffsetY = (screenHeight - scaledProjectHeight) / 2.0f;
    }

    // FIXME: the text is slightly lower on OpenGL
    for (Monitor &var : monitors) {
        if (!var.visible) {
            monitorTexts.erase(var.id);
            listMonitors.erase(var.id);
            continue;
        }

        // Weird Turbowarp math for monitor positions on custom sized projects
        float projectX = (var.x + offsetX) + (EngineState::projectWidth - 480) * 0.5f;
        float projectY = (var.y + offsetY) + (EngineState::projectHeight - 360) * 0.5f;

        if (var.mode == MonitorMode::List) {
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
            float monitorH = var.height * scale;

            const size_t itemsPerPage = std::floor(((monitorH * 0.75) / boxHeight + 4) / 2);
            const size_t start = var.listPage * itemsPerPage;

            const auto &actualList = EntityManager::lists[var.instanceId].orderedKeys[var.varId].items;
            const size_t maxPages = actualList.size() / itemsPerPage;

            const size_t end = std::min(start + itemsPerPage, actualList.size());

            // Draw background
            drawBox(monitorW + (2 * scale), monitorH + (2 * scale), monitorX + (monitorW / 2), monitorY + (monitorH / 2), 194, 204, 217);
            drawBox(monitorW, monitorH, monitorX + (monitorW / 2), monitorY + (monitorH / 2), 229, 240, 255);

            // List name background
            drawBox(monitorW, boxHeight, monitorX + monitorW / 2, monitorY + (boxHeight / 2), 255, 255, 255);

            // List name text
            monitorGfx.name->render(monitorX + (monitorW / 2), monitorY + (4 * scale) + (monitorGfx.name->getSize()[1] / 2));

            // Items
            if (monitorGfx.items.size() != actualList.size()) {
                monitorGfx.items.clear();
                monitorGfx.indices.clear();

                monitorGfx.items.reserve(actualList.size());
                monitorGfx.indices.reserve(actualList.size());

                for (size_t i = start; i < end && i < actualList.size(); ++i) {
                    monitorGfx.items.push_back(createTextObject("", 0, 0));
                    monitorGfx.indices.push_back(createTextObject("", 0, 0));
                }
            }

            if (!actualList.empty()) {
                float item_y = 4 * scale;
                int index = 0;

                for (size_t i = start; i < end && i < actualList.size(); ++i) {
                    const Value &s = actualList[i];
                    drawBox(monitorW - (24 * scale), boxHeight, monitorX + (22 * scale) + (monitorW - (28 * scale)) / 2, monitorY + boxHeight + item_y + (boxHeight / 2), 252, 102, 44);

                    std::unique_ptr<TextObject> &itemText = monitorGfx.items[index];
                    itemText->setText(getListValueString(s));
                    itemText->setColor(Math::color(255, 255, 255, 255));
                    itemText->setScale(1.0f * (scale / 2.0f));
                    itemText->setCenterAligned(false);

                    std::unique_ptr<TextObject> &itemIndexText = monitorGfx.indices[index];
                    itemIndexText->setText(std::to_string(i + 1));
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
            monitorGfx.length->setText("length " + std::to_string(actualList.size()));
            monitorGfx.length->setCenterAligned(true);
            monitorGfx.length->setScale(1.0f * (scale / 2.0f));
            monitorGfx.length->setColor(Math::color(0, 0, 0, 255));
            monitorGfx.length->render(monitorX + (monitorW / 2), monitorY + monitorH - (6 * scale));

            std::array<int, 2> touchPos = Input::getTouchPosition();

            // plus button
            std::unique_ptr<TextObject> plus = createTextObject("+", 0, 0);
            const int plusPosX = monitorX + (8 * scale);
            const int plusPosY = monitorY + monitorH - (14 * scale);
            plus->setCenterAligned(false);
            plus->setColor(Math::color(0, 0, 0, 255));
            plus->setScale(1.0f * (scale / 2.0f));
            plus->render(plusPosX, plusPosY);
#if 1 // adding to lists is an editor only feature
#else
            // This code no longer works because of my second minor runtime update... ( Too lazy to fix it rn)
            if (Input::mousePointer.isPressed && Input::mousePointer.heldFrames == 1 &&
                touchPos[0] > plusPosX && touchPos[0] < plusPosX + plus->getSize()[0] &&
                touchPos[1] > plusPosY && touchPos[1] < plusPosY + plus->getSize()[1]) {
                std::string varValue = Input::openSoftwareKeyboard("Enter new Variable value.");
                if (varValue.empty()) continue;
                for (auto &spr : Scratch::sprites) {
                    if (spr->lists.find(var.id) != spr->lists.end()) {
                        spr->lists[var.id].items.push_back(Value(varValue));
                    }
                }
                var.listPage = static_cast<int>(maxPages);
            }
#endif

            // page buttons
            if (var.listPage < static_cast<int>(maxPages)) {
                std::unique_ptr<TextObject> down = createTextObject("\\/", 0, 0);
                const int downPosX = static_cast<int>(monitorX + monitorW - (18 * scale));
                const int downPosY = static_cast<int>(monitorY + monitorH - (14 * scale));
                down->setCenterAligned(false);
                down->setColor(Math::color(0, 0, 0, 255));
                down->setScale(1.0f * (scale / 2.0f));
                down->render(downPosX, downPosY);
                if (Input::mousePointer.isPressed && Input::mousePointer.heldFrames == 1 &&
                    touchPos[0] > downPosX && touchPos[0] < downPosX + down->getSize()[0] &&
                    touchPos[1] > downPosY && touchPos[1] < downPosY + down->getSize()[1]) {
                    var.listPage = std::clamp(var.listPage + 1, 0, static_cast<int>(maxPages));
                }
            }

            if (var.listPage > 0) {
                std::unique_ptr<TextObject> up = createTextObject("/\\", 0, 0);
                const int upPosX = static_cast<int>(monitorX + monitorW - (8 * scale));
                const int upPosY = static_cast<int>(monitorY + monitorH - (14 * scale));
                up->setCenterAligned(false);
                up->setColor(Math::color(0, 0, 0, 255));
                up->setScale(1.0f * (scale / 2.0f));
                up->render(upPosX, upPosY);
                if (Input::mousePointer.isPressed && Input::mousePointer.heldFrames == 1 &&
                    touchPos[0] > upPosX && touchPos[0] < upPosX + up->getSize()[0] &&
                    touchPos[1] > upPosY && touchPos[1] < upPosY + up->getSize()[1]) {
                    var.listPage = std::clamp(var.listPage - 1, 0, static_cast<int>(maxPages));
                }
            }

        } else {
            std::string renderText = getVariableValueString(var.value);
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
            ColorRGBA valueBackgroundColor = var.color;

            nameObj->setCenterAligned(false);
            valueObj->setCenterAligned(false);

            float baseRenderX = projectX * scale + barOffsetX;
            float baseRenderY = projectY * scale + barOffsetY;

            if (var.mode == MonitorMode::Large) {
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
            } else if (var.mode == MonitorMode::Slider) {
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
                float nameBackgroundHeight = std::max(nameSizeBox[1], valueSizeBox[1]) * 2;
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

                // draw slider
                drawBox(nameBackgroundWidth * 0.97, 9 * scale, nameBackgroundX + nameBackgroundWidth / 2, nameBackgroundY + (8 * scale) + nameBackgroundHeight / 2, 178, 178, 178, 255);
                drawBox(nameBackgroundWidth * 0.95, 7 * scale, nameBackgroundX + nameBackgroundWidth / 2, nameBackgroundY + (8 * scale) + nameBackgroundHeight / 2, 239, 239, 239, 255);

                const int minPos = nameBackgroundX + 4 * scale;
                const int maxPos = nameBackgroundX + nameBackgroundWidth;
                const double sliderMin = var.sliderMin;
                const double sliderMax = var.sliderMax;
                const double value = var.value.asDouble();
                const int sliderPos = std::clamp(static_cast<int>(minPos + (value - sliderMin) * (maxPos - minPos) / (sliderMax - sliderMin)), minPos, maxPos);

                drawBox(13 * scale, 13 * scale, sliderPos, nameBackgroundY + (8 * scale) + nameBackgroundHeight / 2, 0, 115, 252, 255);

                std::array<int, 2> touchPos = Input::getTouchPosition();

                if (Input::mousePointer.isPressed && touchPos[0] > nameBackgroundX && touchPos[0] < nameBackgroundX + nameBackgroundWidth &&
                    touchPos[1] > nameBackgroundY + (8 * scale) + (7 * scale) && touchPos[1] < nameBackgroundY + (8 * scale) + (7 * scale) * 3) {

                    const int clampedX = std::clamp(touchPos[0], minPos, maxPos);

                    const double normalized = static_cast<double>(clampedX - minPos) / static_cast<double>(maxPos - minPos);

                    double newValue = sliderMin + normalized * (sliderMax - sliderMin);

                    if (var.isDiscrete) {
                        newValue = static_cast<int>(newValue);
                    } else newValue = std::round(newValue * 100.0) / 100.0;

                    // snap to edges
                    if (clampedX <= minPos + 5 * scale) {
                        newValue = sliderMin;
                    } else if (clampedX >= maxPos - 5 * scale) {
                        newValue = sliderMax;
                    }

                    // not sure if any other monitor types can be sliders,
                    // Br0t: I hope so, otherwise I'll have to change some things again :(
                    var.value = Value(newValue);
                    if (var.varId != EMPTY_VAR_ID) {
                        EntityManager::variables[var.instanceId].orderedKeys[var.varId].value = Value(newValue);
                    }
                }

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
    }
}

std::pair<float, float> Render::screenToScratchCoords(float screenX, float screenY, int windowWidth, int windowHeight) {
#ifdef RENDERER_CITRO2D
    if (Render::renderMode == Render::BOTH_SCREENS) {
        windowWidth = 400;
        windowHeight = 480;
    }
#endif

    const float projWidth = static_cast<float>(EngineState::projectWidth);
    const float projHeight = static_cast<float>(EngineState::projectHeight);

    float screenAspect = static_cast<float>(windowWidth) / windowHeight;
    float projectAspect = projWidth / projHeight;

    float scratchX, scratchY;

    if (screenAspect > projectAspect) {
        float scale = static_cast<float>(windowHeight) / projHeight;
        float scaledProjectWidth = projWidth * scale;
        float barWidth = (windowWidth - scaledProjectWidth) / 2.0f;

        float adjustedX = screenX - barWidth;
        scratchX = (adjustedX / scaledProjectWidth) * projWidth - (projWidth / 2.0f);
        scratchY = (projHeight / 2.0f) - (screenY / windowHeight) * projHeight;

    } else if (screenAspect < projectAspect) {
        float scale = static_cast<float>(windowWidth) / projWidth;
        float scaledProjectHeight = projHeight * scale;
        float barHeight = (windowHeight - scaledProjectHeight) / 2.0f;

        float adjustedY = screenY - barHeight;
        scratchX = (screenX / windowWidth) * projWidth - (projWidth / 2.0f);
        scratchY = (projHeight / 2.0f) - (adjustedY / scaledProjectHeight) * projHeight;

    } else {
        float scale = static_cast<float>(windowWidth) / projWidth;
        scratchX = (screenX / scale) - (projWidth / 2.0f);
        scratchY = (projHeight / 2.0f) - (screenY / scale);
#ifdef RENDERER_CITRO2D
        if (Render::renderMode == Render::BOTH_SCREENS) {
            scratchY -= 120;
        }
#endif
    }

    return {scratchX, scratchY};
}