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
SCRATCH_BLOCK_SIGNATURE(sensing, resettimer);
SCRATCH_BLOCK_POSITIONAL_VOID(sensing, resettimer) {
    BlockExecutor::timer.start();
}

SCRATCH_BLOCK(sensing, askandwait) {
    std::string input;
    if (!Scratch::getInputValueAs(block, "QUESTION", thread, sprite, input)) return BlockResult::REPEAT;
    Scratch::answer = Input::openSoftwareKeyboard(input.c_str());

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(sensing, askandwait, {"QUESTION", BytecodeStackKind::String});
SCRATCH_BLOCK_POSITIONAL_S_VOID(sensing, askandwait) {
    Scratch::answer = Input::openSoftwareKeyboard(str.c_str());
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
SCRATCH_BYTECODE_BLOCK(sensing, setdragmode) {
    const std::string mode = Scratch::getFieldValue(*block, "DRAG_MODE");
    BytecodeIndex draggable = (mode == "draggable") ? 1 : 0;

    outFirstInstr = compiler.emit(BytecodeOp::SetDragMode, invalidBytecodeIndex, draggable);
    return true;
}

SCRATCH_BLOCK_POSITIONAL_SPRITE_N(sensing, timer) {
    return BlockExecutor::timer.getTimeMs() / 1000.0;
}
SCRATCH_BLOCK_DOUBLE(sensing, timer) {
    *outValue = block_sensing_timer_pos_(sprite);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(sensing, timer);

SCRATCH_BLOCK_POSITIONAL_SPRITE_SS_V(sensing, of) {
    const std::string &object = str1;
    const std::string &value = str2;
    Value result(0);

    Sprite *spriteObject = nullptr;
    for (Sprite *currentSprite : Scratch::sprites) {
        if (!currentSprite->isClone && (currentSprite->name == object || (object == "_stage_" && currentSprite->isStage))) {
            spriteObject = currentSprite;
            break;
        }
    }

    if (!spriteObject) return result;

    const auto setOutFromVar = [&result](const Variable &variable) {
        std::visit([&result](auto &&v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, Value>) {
                result = v;
            } else {
                result = Value(v);
            }
        },
                   variable.value);
    };

    if (spriteObject->isStage) {
        if (value == "background #" || value == "backdrop #") {
            result = Value(spriteObject->currentCostume + 1);
        } else if (value == "backdrop name") {
            result = Value(spriteObject->costumes[spriteObject->currentCostume].name);
        } else {
            for (const auto &[id, variable] : spriteObject->variables) {
                if (value == variable.name) {
                    setOutFromVar(variable);
                    break;
                }
            }
        }
    } else {
        if (value == "x position") result = Value(spriteObject->xPosition);
        else if (value == "y position") result = Value(spriteObject->yPosition);
        else if (value == "direction") result = Value(spriteObject->rotation);
        else if (value == "costume #") result = Value(spriteObject->currentCostume + 1);
        else if (value == "costume name" || value == "backdrop name") result = Value(spriteObject->costumes[spriteObject->currentCostume].name);
        else if (value == "size") result = Value(spriteObject->size);
        else {
            for (const auto &[id, variable] : spriteObject->variables) {
                if (value == variable.name) {
                    setOutFromVar(variable);
                    break;
                }
            }
        }
    }
    return result;
}
SCRATCH_BLOCK(sensing, of) {
    std::string object;
    if (!Scratch::getInputValueAs(block, "OBJECT", thread, sprite, object)) return BlockResult::REPEAT;

    std::string value;
    if (!Scratch::getInputValueAs(block, "PROPERTY", thread, sprite, value)) return BlockResult::REPEAT;
    *outValue = block_sensing_of_pos_(sprite, object, value);

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(sensing, of, {"OBJECT", BytecodeStackKind::String}, {"PROPERTY", BytecodeStackKind::String});

SCRATCH_BLOCK_POSITIONAL_SPRITE_N(sensing, mousex) {
    return Input::mousePointer.x;
}
SCRATCH_BLOCK_DOUBLE(sensing, mousex) {
    *outValue = block_sensing_mousex_pos_(sprite);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(sensing, mousex);

SCRATCH_BLOCK_POSITIONAL_SPRITE_N(sensing, mousey) {
    return Input::mousePointer.y;
}
SCRATCH_BLOCK_DOUBLE(sensing, mousey) {
    *outValue = block_sensing_mousey_pos_(sprite);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(sensing, mousey);

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
SCRATCH_BLOCK_SIGNATURE(sensing, distanceto, {"DISTANCETOMENU", BytecodeStackKind::String});
SCRATCH_BLOCK_POSITIONAL_SPRITE_S_N(sensing, distanceto) {
    if (str == "_mouse_") {
        const double dx = Input::mousePointer.x - sprite->xPosition;
        const double dy = Input::mousePointer.y - sprite->yPosition;
        return std::sqrt(dx * dx + dy * dy);
    }

    for (Sprite *currentSprite : Scratch::sprites) {
        if (currentSprite->isClone || currentSprite->name != str) continue;
        const double dx = currentSprite->xPosition - sprite->xPosition;
        const double dy = currentSprite->yPosition - sprite->yPosition;
        return std::sqrt(dx * dx + dy * dy);
    }
    return 10000;
}

SCRATCH_BLOCK_POSITIONAL_SPRITE_N(sensing, dayssince2000) {
    return TimeSE::getDaysSince2000();
}
SCRATCH_BLOCK_DOUBLE(sensing, dayssince2000) {
    *outValue = block_sensing_dayssince2000_pos_(sprite);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(sensing, dayssince2000);

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
SCRATCH_BYTECODE_BLOCK(sensing, current) {
    const std::string inputValue = Scratch::getFieldValue(*block, "CURRENTMENU");

    BytecodeIndex field;
    if (inputValue == "YEAR") field = 0;
    else if (inputValue == "MONTH") field = 1;
    else if (inputValue == "DATE") field = 2;
    else if (inputValue == "DAYOFWEEK") field = 3;
    else if (inputValue == "HOUR") field = 4;
    else if (inputValue == "MINUTE") field = 5;
    else if (inputValue == "SECOND") field = 6;
    else return false;

    outFirstInstr = compiler.emit(BytecodeOp::CurrentDateTime, invalidBytecodeIndex, field);
    return true;
}

SCRATCH_BLOCK_POSITIONAL_SPRITE_S(sensing, answer) {
    return Scratch::answer;
}
SCRATCH_BLOCK_STRING(sensing, answer) {
    *outValue = block_sensing_answer_pos_(sprite);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(sensing, answer);

SCRATCH_BLOCK_POSITIONAL_V_B(sensing, keypressed) {
    const Value &keyOption = value;
    for (const std::string &button : Input::inputKeys) {
        if (Input::convertToKey(keyOption) == button) return true;
    }
    return false;
}
SCRATCH_BLOCK_BOOLEAN(sensing, keypressed) {
    Value keyOption;
    if (!Scratch::getInputValue(block, "KEY_OPTION", thread, sprite, keyOption)) return BlockResult::REPEAT;
    *outValue = block_sensing_keypressed_pos_(keyOption);

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(sensing, keypressed, {"KEY_OPTION", BytecodeStackKind::Value});

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
SCRATCH_BLOCK_SIGNATURE(sensing, touchingobject, {"TOUCHINGOBJECTMENU", BytecodeStackKind::String});
SCRATCH_BLOCK_POSITIONAL_SPRITE_S_B(sensing, touchingobject) {
    if (str == "_mouse_") return Scratch::isColliding("mouse", sprite);
    if (str == "_edge_") return Scratch::isColliding("edge", sprite);

    for (size_t i = 0; i < Scratch::sprites.size(); i++) {
        Sprite *currentSprite = Scratch::sprites[i];
        if (currentSprite == sprite) continue;
        if (currentSprite->name == str && Scratch::isColliding("sprite", sprite, currentSprite, str)) return true;
    }
    return false;
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
SCRATCH_BLOCK_SIGNATURE(sensing, touchingcolor, {"COLOR", BytecodeStackKind::Value});

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
SCRATCH_BLOCK_SIGNATURE(sensing, coloristouchingcolor, {"COLOR", BytecodeStackKind::Value}, {"COLOR2", BytecodeStackKind::Value});

SCRATCH_BLOCK_POSITIONAL_SPRITE_B(sensing, mousedown) {
    return Input::mousePointer.isPressed;
}
SCRATCH_BLOCK_BOOLEAN(sensing, mousedown) {
    *outValue = block_sensing_mousedown_pos_(sprite);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(sensing, mousedown);

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
SCRATCH_BLOCK_SIGNATURE(sensing, username);

SCRATCH_BLOCK_BOOLEAN(sensing, online) {
    *outValue = OS::isOnline();
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(sensing, online);

SCRATCH_BLOCK(sensing, userid) {
    *outValue = Value(Undefined{});
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(sensing, userid);

SCRATCH_SHADOW_BLOCK(sensing_touchingobjectmenu, TOUCHINGOBJECTMENU)
SCRATCH_SHADOW_BLOCK(sensing_distancetomenu, DISTANCETOMENU)
SCRATCH_SHADOW_BLOCK(sensing_keyoptions, KEY_OPTION)
SCRATCH_SHADOW_BLOCK(sensing_of_object_menu, OBJECT)
