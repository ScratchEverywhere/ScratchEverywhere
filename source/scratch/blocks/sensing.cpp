#include "sensing.hpp"
#include "blockExecutor.hpp"
#include "input.hpp"
#include "interpret.hpp"
#include "keyboard.hpp"
#include "sprite.hpp"
#include "value.hpp"
#include <cmath>
#include <utility>
#include <vector>

BlockResult SensingBlocks::resetTimer(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    BlockExecutor::timer.start();
    return BlockResult::CONTINUE;
}

BlockResult SensingBlocks::askAndWait(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Keyboard kbd;
    Value inputValue = Scratch::getInputValue(block, "QUESTION", sprite);
    std::string output = kbd.openKeyboard(inputValue.asString().c_str());
    answer = output;
    return BlockResult::CONTINUE;
}

BlockResult SensingBlocks::setDragMode(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {

    std::string mode = Scratch::getFieldValue(block, "DRAG_MODE");
    ;

    if (mode == "draggable") {
        sprite->draggable = true;
    } else if (mode == "not draggable") {
        sprite->draggable = false;
    }

    return BlockResult::CONTINUE;
}

Value SensingBlocks::sensingTimer(Block &block, Sprite *sprite) {
    return Value(BlockExecutor::timer.getTimeMs() / 1000.0);
}

Value SensingBlocks::of(Block &block, Sprite *sprite) {
    std::string value = Scratch::getFieldValue(block, "PROPERTY");
    Value inputValue = Scratch::getInputValue(block, "OBJECT", sprite);

    Sprite *spriteObject = nullptr;
    for (Sprite *currentSprite : sprites) {
        if (currentSprite->name == inputValue.asString() && !currentSprite->isClone) {
            spriteObject = currentSprite;
            break;
        }
    }

    if (!spriteObject) return Value(0);

    if (value == "timer") {
        return Value(BlockExecutor::timer.getTimeMs() / 1000);
    } else if (value == "x position") {
        return Value(spriteObject->xPosition);
    } else if (value == "y position") {
        return Value(spriteObject->yPosition);
    } else if (value == "direction") {
        return Value(spriteObject->rotation);
    } else if (value == "costume #" || value == "backdrop #") {
        return Value(spriteObject->currentCostume + 1);
    } else if (value == "costume name" || value == "backdrop name") {
        return Value(spriteObject->costumes[spriteObject->currentCostume].name);
    } else if (value == "size") {
        return Value(spriteObject->size);
    } else if (value == "volume") {
        return Value(spriteObject->volume);
    }

    for (const auto &[id, variable] : spriteObject->variables) {
        if (value == variable.name) {
            return variable.value;
        }
    }
    return Value(0);
}

Value SensingBlocks::mouseX(Block &block, Sprite *sprite) {
    return Value(Input::mousePointer.x);
}

Value SensingBlocks::mouseY(Block &block, Sprite *sprite) {
    return Value(Input::mousePointer.y);
}

Value SensingBlocks::distanceTo(Block &block, Sprite *sprite) {
    Value inputValue = Scratch::getInputValue(block, "DISTANCETOMENU", sprite);

    if (inputValue.asString() == "_mouse_") {
        return Value(sqrt(pow(Input::mousePointer.x - sprite->xPosition, 2) +
                          pow(Input::mousePointer.y - sprite->yPosition, 2)));
    }

    for (Sprite *currentSprite : sprites) {
        if (currentSprite->name == inputValue.asString() && !currentSprite->isClone) {
            double distance = sqrt(pow(currentSprite->xPosition - sprite->xPosition, 2) +
                                   pow(currentSprite->yPosition - sprite->yPosition, 2));
            return Value(distance);
        }
    }
    return Value(10000);
}

Value SensingBlocks::daysSince2000(Block &block, Sprite *sprite) {
    return Value(Time::getDaysSince2000());
}

Value SensingBlocks::current(Block &block, Sprite *sprite) {
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

Value SensingBlocks::sensingAnswer(Block &block, Sprite *sprite) {
    return Value(answer);
}

Value SensingBlocks::keyPressed(Block &block, Sprite *sprite) {
    const Value keyName = Scratch::getInputValue(block, "KEY_OPTION", sprite);
    for (std::string button : Input::inputButtons) {
        if (Input::convertToKey(keyName) == button) {
            return Value(true);
        }
    }
    return Value(false);
}

Value SensingBlocks::touchingObject(Block &block, Sprite *sprite) {
    Value inputValue = Scratch::getInputValue(block, "TOUCHINGOBJECTMENU", sprite);

    if (inputValue.asString() == "_mouse_") {
        return Value(isColliding("mouse", sprite));
    } else if (inputValue.asString() == "_edge_") {
        return Value(isColliding("edge", sprite));
    } else {
        for (size_t i = 0; i < sprites.size(); i++) {
            Sprite *currentSprite = sprites[i];
            if (currentSprite == sprite) continue;
            if (currentSprite->name == inputValue.asString() &&
                isColliding("sprite", sprite, currentSprite, inputValue.asString())) {
                return Value(true);
            }
        }
    }
    return Value(false);
}

Value SensingBlocks::mouseDown(Block &block, Sprite *sprite) {
    return Value(Input::mousePointer.isPressed);
}

Value SensingBlocks::username(Block &block, Sprite *sprite) {
    return Value(Input::getUsername());
}
