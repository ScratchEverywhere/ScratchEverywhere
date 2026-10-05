#include "blockUtils.hpp"
#include <algorithm>
#include <cmath>
#include <collision.hpp>
#include <input.hpp>
#include <types.hpp>
#include <utility>
#include <value.hpp>
#include <vector>

SCRATCH_BLOCK(sensing, resettimer) {
    BlockExecutor::timer.start();
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, askandwait) {
    std::string input;
    if (!Scratch::getInputValueAs(block, "QUESTION", thread, sprite, input)) return BlockResult::REPEAT;
    Scratch::answer = Input::openSoftwareKeyboard(input.c_str());

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, setdragmode) {
    const std::string mode = Scratch::getFieldValue(*block, "DRAG_MODE");

    if (mode == "draggable") {
        sprite->draggable = true;
    } else if (mode == "not draggable") {
        sprite->draggable = false;
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_DOUBLE(sensing, timer) {
    *outValue = BlockExecutor::timer.getTimeMs() / 1000.0;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, of) {
    std::string object;
    if (!Scratch::getInputValueAs(block, "OBJECT", thread, sprite, object)) return BlockResult::REPEAT;

    const std::string value = Scratch::getFieldValue(*block, "PROPERTY");
    *outValue = Value(0);

    Sprite *spriteObject = nullptr;
    for (Sprite *currentSprite : Scratch::sprites) {
        if (!currentSprite->isClone && (currentSprite->name == object || (object == "_stage_" && currentSprite->isStage))) {
            spriteObject = currentSprite;
            break;
        }
    }

    if (!spriteObject) return BlockResult::CONTINUE;

    const auto setOutFromVar = [outValue](const Variable &variable) {
        std::visit([outValue](auto &&v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, Value>) {
                *outValue = v;
            } else {
                *outValue = Value(v);
            }
        },
                   variable.value);
    };

    if (spriteObject->isStage) {
        if (value == "background #" || value == "backdrop #") {
            *outValue = Value(spriteObject->currentCostume + 1);
        } else if (value == "backdrop name") {
            *outValue = Value(spriteObject->costumes[spriteObject->currentCostume].name);
        } else {
            for (const auto &[id, variable] : spriteObject->variables) {
                if (value == variable.name) {
                    setOutFromVar(variable);
                    break;
                }
            }
        }
    } else {
        if (value == "x position") *outValue = Value(spriteObject->xPosition);
        else if (value == "y position") *outValue = Value(spriteObject->yPosition);
        else if (value == "direction") *outValue = Value(spriteObject->rotation);
        else if (value == "costume #") *outValue = Value(spriteObject->currentCostume + 1);
        else if (value == "costume name" || value == "backdrop name") *outValue = Value(spriteObject->costumes[spriteObject->currentCostume].name);
        else if (value == "size") *outValue = Value(spriteObject->size);
        else {
            for (const auto &[id, variable] : spriteObject->variables) {
                if (value == variable.name) {
                    setOutFromVar(variable);
                    break;
                }
            }
        }
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_DOUBLE(sensing, mousex) {
    *outValue = Input::mousePointer.x;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_DOUBLE(sensing, mousey) {
    *outValue = Input::mousePointer.y;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_DOUBLE(sensing, distanceto) {
    std::string distanceTo;
    if (!Scratch::getInputValueAs(block, "DISTANCETOMENU", thread, sprite, distanceTo)) return BlockResult::REPEAT;

    if (distanceTo == "_mouse_") {
        const double dx = Input::mousePointer.x - sprite->xPosition;
        const double dy = Input::mousePointer.y - sprite->yPosition;
        *outValue = std::sqrt(dx * dx + dy * dy);
        return BlockResult::CONTINUE;
    }

    for (Sprite *currentSprite : Scratch::sprites) {
        if (currentSprite->isClone || currentSprite->name != distanceTo) continue;
        const double dx = currentSprite->xPosition - sprite->xPosition;
        const double dy = currentSprite->yPosition - sprite->yPosition;
        *outValue = std::sqrt(dx * dx + dy * dy);
        return BlockResult::CONTINUE;
    }
    *outValue = 10000;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_DOUBLE(sensing, dayssince2000) {
    *outValue = TimeSE::getDaysSince2000();
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_DOUBLE(sensing, current) {
    std::string inputValue = Scratch::getFieldValue(*block, "CURRENTMENU");

    if (inputValue == "YEAR") *outValue = TimeSE::getYear();
    else if (inputValue == "MONTH") *outValue = TimeSE::getMonth();
    else if (inputValue == "DATE") *outValue = TimeSE::getDay();
    else if (inputValue == "DAYOFWEEK") *outValue = TimeSE::getDayOfWeek();
    else if (inputValue == "HOUR") *outValue = TimeSE::getHours();
    else if (inputValue == "MINUTE") *outValue = TimeSE::getMinutes();
    else if (inputValue == "SECOND") *outValue = TimeSE::getSeconds();
    else *outValue = 0;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_STRING(sensing, answer) {
    *outValue = Scratch::answer;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_BOOLEAN(sensing, keypressed) {
    Value keyOption;
    if (!Scratch::getInputValue(block, "KEY_OPTION", thread, sprite, keyOption)) return BlockResult::REPEAT;
    *outValue = false;

    for (std::string button : Input::inputKeys) {
        if (Input::convertToKey(keyOption) == button) {
            *outValue = true;
            break;
        }
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_BOOLEAN(sensing, touchingobject) {
    std::string touchingObject;
    if (!Scratch::getInputValueAs(block, "TOUCHINGOBJECTMENU", thread, sprite, touchingObject)) return BlockResult::REPEAT;

    if (touchingObject == "_mouse_")
        *outValue = Scratch::isColliding("mouse", sprite);
    else if (touchingObject == "_edge_")
        *outValue = Scratch::isColliding("edge", sprite);
    else {
        *outValue = false;
        for (size_t i = 0; i < Scratch::sprites.size(); i++) {
            Sprite *currentSprite = Scratch::sprites[i];
            if (currentSprite == sprite) continue;
            if (currentSprite->name == touchingObject &&
                Scratch::isColliding("sprite", sprite, currentSprite, touchingObject)) {
                *outValue = true;
                return BlockResult::CONTINUE;
            }
        }
    }
    return BlockResult::CONTINUE;
}

static uint8_t toColorByte(float channel) {
    return static_cast<uint8_t>(std::clamp(std::round(channel), 0.0f, 255.0f));
}

SCRATCH_BLOCK_BOOLEAN(sensing, touchingcolor) {
    Value targetColor;
    if (!Scratch::getInputValue(block, "COLOR", thread, sprite, targetColor)) return BlockResult::REPEAT;
    const ColorRGBA rgb = targetColor.asRGB();

    *outValue = collision::isTouchingColor(sprite, toColorByte(rgb.r), toColorByte(rgb.g), toColorByte(rgb.b));
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_BOOLEAN(sensing, coloristouchingcolor) {
    Value maskColor;
    if (!Scratch::getInputValue(block, "COLOR", thread, sprite, maskColor)) return BlockResult::REPEAT;
    Value targetColor;
    if (!Scratch::getInputValue(block, "COLOR2", thread, sprite, targetColor)) return BlockResult::REPEAT;

    const ColorRGBA maskRgb = maskColor.asRGB();
    const ColorRGBA targetRgb = targetColor.asRGB();

    *outValue = collision::colorIsTouchingColor(sprite,
                                                toColorByte(maskRgb.r), toColorByte(maskRgb.g), toColorByte(maskRgb.b),
                                                toColorByte(targetRgb.r), toColorByte(targetRgb.g), toColorByte(targetRgb.b));
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_BOOLEAN(sensing, mousedown) {
    *outValue = Input::mousePointer.isPressed;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_STRING(sensing, username) {
#ifdef ENABLE_CLOUDVARS
    if (Scratch::cloudProject) *outValue = Scratch::cloudUsername;
    else
#endif
        if (Scratch::useCustomUsername)
        *outValue = Scratch::customUsername;
    else *outValue = OS::getUsername();
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_BOOLEAN(sensing, online) {
    *outValue = OS::isOnline();
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, userid) {
    *outValue = Value(Undefined{});
    return BlockResult::CONTINUE;
}

SCRATCH_SHADOW_BLOCK(sensing_touchingobjectmenu, TOUCHINGOBJECTMENU)
SCRATCH_SHADOW_BLOCK(sensing_distancetomenu, DISTANCETOMENU)
SCRATCH_SHADOW_BLOCK(sensing_keyoptions, KEY_OPTION)
SCRATCH_SHADOW_BLOCK(sensing_of_object_menu, OBJECT)
