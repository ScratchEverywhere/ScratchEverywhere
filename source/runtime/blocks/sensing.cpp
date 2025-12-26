#include "blockUtils.hpp"
#include <cmath>
#include <input.hpp>
#include <interpret.hpp>
#include <keyboard.hpp>
#include <sprite.hpp>
#include <utility>
#include <value.hpp>
#include <vector>

SCRATCH_BLOCK(sensing, resettimer) {
    BlockExecutor::timer.start();
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, askandwait) {
    SoftwareKeyboard kbd;
    const Value inputValue = Scratch::getInputValue(block, "QUESTION", sprite);
    answer = kbd.openKeyboard(inputValue.asString().c_str());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, setdragmode) {
    const std::string mode = Scratch::getFieldValue(block, "DRAG_MODE");

    if (mode == "draggable") {
        sprite->draggable = true;
    } else if (mode == "not draggable") {
        sprite->draggable = false;
    }

    return BlockResult::CONTINUE;
}

SCRATCH_REPORTER_BLOCK(sensing, timer) {
    return Value(BlockExecutor::timer.getTimeMs() / 1000.0);
}

SCRATCH_REPORTER_BLOCK(sensing, of) {
    const std::string value = Scratch::getFieldValue(block, "PROPERTY");
    const Value inputValue = Scratch::getInputValue(block, "OBJECT", sprite);

    Sprite *spriteObject = nullptr;
    for (Sprite *currentSprite : sprites) {
        if ((currentSprite->name == inputValue.asString() || (inputValue.asString() == "_stage_" && currentSprite->isStage)) && !currentSprite->isClone) {
            spriteObject = currentSprite;
            break;
        }
    }

    if (!spriteObject) return Value(0);

    if (spriteObject->isStage) {
        if (value == "background #") return Value(spriteObject->currentCostume + 1);
        if (value == "backdrop #") return Value(spriteObject->currentCostume + 1);
        if (value == "backdrop name") return Value(spriteObject->costumes[spriteObject->currentCostume].name);
    } else {
        if (value == "x position") return Value(spriteObject->xPosition);
        if (value == "y position") return Value(spriteObject->yPosition);
        if (value == "direction") return Value(spriteObject->rotation);
        if (value == "costume #") return Value(spriteObject->currentCostume + 1);
        if (value == "costume name") return Value(spriteObject->costumes[spriteObject->currentCostume].name);
        if (value == "backdrop name") return Value(spriteObject->costumes[spriteObject->currentCostume].name);
        if (value == "size") return Value(spriteObject->size);
    }

    for (const auto &[id, variable] : spriteObject->variables) {
        if (value == variable.name) return variable.value;
    }
    return Value(0);
}

SCRATCH_REPORTER_BLOCK(sensing, mousex) {
    return Value(Input::mousePointer.x);
}

SCRATCH_REPORTER_BLOCK(sensing, mousey) {
    return Value(Input::mousePointer.y);
}

SCRATCH_REPORTER_BLOCK(sensing, distanceto) {
    const Value inputValue = Scratch::getInputValue(block, "DISTANCETOMENU", sprite);

    if (inputValue.asString() == "_mouse_") {
        const double dx = Input::mousePointer.x - sprite->xPosition;
        const double dy = Input::mousePointer.y - sprite->yPosition;
        return Value(std::sqrt(dx * dx + dy * dy));
    }

    for (Sprite *currentSprite : sprites) {
        if (currentSprite->name != inputValue.asString() || currentSprite->isClone) continue;
        const double dx = currentSprite->xPosition - sprite->xPosition;
        const double dy = currentSprite->yPosition - sprite->yPosition;
        return Value(std::sqrt(dx * dx + dy * dy));
    }

    return Value(10000);
}

SCRATCH_REPORTER_BLOCK(sensing, dayssince2000) {
    return Value(Time::getDaysSince2000());
}

SCRATCH_REPORTER_BLOCK(sensing, current) {
    std::string inputValue;
    try {
        inputValue = Scratch::getFieldValue(block, "CURRENTMENU");
    } catch (...) {
        return Value();
    }

    if (inputValue == "YEAR") return Value(Time::getYear());
    if (inputValue == "MONTH") return Value(Time::getMonth());
    if (inputValue == "DATE") return Value(Time::getDay());
    if (inputValue == "DAYOFWEEK") return Value(Time::getDayOfWeek());
    if (inputValue == "HOUR") return Value(Time::getHours());
    if (inputValue == "MINUTE") return Value(Time::getMinutes());
    if (inputValue == "SECOND") return Value(Time::getSeconds());

    return Value();
}

SCRATCH_REPORTER_BLOCK(sensing, answer) {
    return Value(answer);
}

SCRATCH_REPORTER_BLOCK(sensing, keypressed) {
    const Value keyName = Scratch::getInputValue(block, "KEY_OPTION", sprite);
    for (std::string button : Input::inputButtons) {
        if (Input::convertToKey(keyName) == button) return Value(true);
    }

    return Value(false);
}

SCRATCH_REPORTER_BLOCK(sensing, touchingobject) {
    const Value inputValue = Scratch::getInputValue(block, "TOUCHINGOBJECTMENU", sprite);

    if (inputValue.asString() == "_mouse_") return Value(isColliding("mouse", sprite));
    if (inputValue.asString() == "_edge_") return Value(isColliding("edge", sprite));

    for (size_t i = 0; i < sprites.size(); i++) {
        Sprite *currentSprite = sprites[i];
        if (currentSprite == sprite) continue;
        if (currentSprite->name == inputValue.asString() &&
            isColliding("sprite", sprite, currentSprite, inputValue.asString())) {
            return Value(true);
        }
    }
    return Value(false);
}

SCRATCH_REPORTER_BLOCK(sensing, mousedown) {
    return Value(Input::mousePointer.isPressed);
}

SCRATCH_REPORTER_BLOCK(sensing, username) {
    return Value(Input::getUsername());
}
