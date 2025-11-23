#include "blockUtils.hpp"
#include "input.hpp"
#include "interpret.hpp"
#include "keyboard.hpp"
#include "sprite.hpp"
#include "value.hpp"
#include <cmath>
#include <utility>
#include <vector>

namespace blocks::sensing {
SCRATCH_BLOCK(sensing, resettimer) {
    BlockExecutor::timer.start();
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, askandwait) {
    Keyboard kbd;
    Value inputValue = Scratch::getInputValue(block, "QUESTION", sprite);
    std::string output = kbd.openKeyboard(inputValue.asString().c_str());
    answer = output;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, setdragmode) {

    std::string mode = Scratch::getFieldValue(block, "DRAG_MODE");
    ;

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
    std::string value = Scratch::getFieldValue(block, "PROPERTY");
    std::string object;
    auto objectFind = block.parsedInputs->find("OBJECT");
    Block *objectBlock = findBlock(objectFind->second.literalValue.asString());
    if (!objectBlock || objectBlock == nullptr)
        return Value();

    object = Scratch::getFieldValue(*objectBlock, "OBJECT");

    Sprite *spriteObject = nullptr;
    for (Sprite *currentSprite : sprites) {
        if (currentSprite->name == object && !currentSprite->isClone) {
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

SCRATCH_REPORTER_BLOCK(sensing, mousex) {
    return Value(Input::mousePointer.x);
}

SCRATCH_REPORTER_BLOCK(sensing, mousey) {
    return Value(Input::mousePointer.y);
}

SCRATCH_REPORTER_BLOCK(sensing, distanceto) {
    auto inputFind = block.parsedInputs->find("DISTANCETOMENU");
    Block *inputBlock = findBlock(inputFind->second.literalValue.asString());
    std::string object = Scratch::getFieldValue(*inputBlock, "DISTANCETOMENU");

    if (object == "_mouse_") {
        return Value(sqrt(pow(Input::mousePointer.x - sprite->xPosition, 2) +
                          pow(Input::mousePointer.y - sprite->yPosition, 2)));
    }

    for (Sprite *currentSprite : sprites) {
        if (currentSprite->name == object && !currentSprite->isClone) {
            double distance = sqrt(pow(currentSprite->xPosition - sprite->xPosition, 2) +
                                   pow(currentSprite->yPosition - sprite->yPosition, 2));
            return Value(distance);
        }
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
        ;
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
    auto inputFind = block.parsedInputs->find("KEY_OPTION");
    std::string buttonCheck;

    // if no variable block is in the input
    if (inputFind->second.inputType == ParsedInput::LITERAL) {
        Block *inputBlock = findBlock(inputFind->second.literalValue.asString());
        if (inputBlock == nullptr) return Value(false);
        if (Scratch::getFieldValue(*inputBlock, "KEY_OPTION") != "")
            buttonCheck = Scratch::getFieldValue(*inputBlock, "KEY_OPTION");
    } else {
        buttonCheck = Scratch::getInputValue(block, "KEY_OPTION", sprite).asString();
    }

    for (std::string button : Input::inputButtons) {
        if (buttonCheck == button) {
            return Value(true);
        }
    }

    return Value(false);
}

SCRATCH_REPORTER_BLOCK(sensing, touchingobject) {
    auto inputFind = block.parsedInputs->find("TOUCHINGOBJECTMENU");
    Block *inputBlock = findBlock(inputFind->second.literalValue.asString());
    std::string objectName;
    try {
        objectName = Scratch::getFieldValue(*inputBlock, "TOUCHINGOBJECTMENU");
    } catch (...) {
        return Value(false);
    }

    if (objectName == "_mouse_") {
        return Value(isColliding("mouse", sprite));
    } else if (objectName == "_edge_") {
        return Value(isColliding("edge", sprite));
    } else {
        for (size_t i = 0; i < sprites.size(); i++) {
            Sprite *currentSprite = sprites[i];
            if (currentSprite->name == objectName &&
                isColliding("sprite", sprite, currentSprite, objectName)) {
                return Value(true);
            }
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
} // namespace blocks::sensing
