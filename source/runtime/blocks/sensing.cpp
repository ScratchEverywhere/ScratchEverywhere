#include "blockUtils.hpp"
#include <cmath>
#include <input.hpp>
#include <sprite.hpp>
#include <utility>
#include <value.hpp>
#include <vector>

SCRATCH_BLOCK(sensing, resettimer) {
    BlockExecutor::timer.start();
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, askandwait) {
    Value input;
    if (!Scratch::getInput(block, "QUESTION", thread, sprite, input)) return BlockResult::REPEAT;
    Scratch::answer = Input::openSoftwareKeyboard(input.asString().c_str());

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

SCRATCH_BLOCK(sensing, timer) {
    *outValue = Value(BlockExecutor::timer.getTimeMs() / 1000.0);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, of) {
    Value object;
    if (!Scratch::getInput(block, "OBJECT", thread, sprite, object)) return BlockResult::REPEAT;

    const std::string value = Scratch::getFieldValue(*block, "PROPERTY");
    *outValue = Value(0);

    Sprite *spriteObject = nullptr;
    for (Sprite *currentSprite : Scratch::sprites) {
        if (!currentSprite->isClone && (currentSprite->name == object.asString() || (object.asString() == "_stage_" && currentSprite->isStage))) {
            spriteObject = currentSprite;
            break;
        }
    }

    if (!spriteObject) return BlockResult::CONTINUE;

    if (spriteObject->isStage) {
        if (value == "background #") *outValue = Value(spriteObject->currentCostume + 1);
        else if (value == "backdrop #") *outValue = Value(spriteObject->currentCostume + 1);
        else if (value == "backdrop name") *outValue = Value(spriteObject->costumes[spriteObject->currentCostume].name);
        else {
            for (const auto &[id, variable] : spriteObject->variables) {
                if (value == variable.name) *outValue = variable.value;
            }
        }
    } else {
        if (value == "x position") *outValue = Value(spriteObject->xPosition);
        else if (value == "y position") *outValue = Value(spriteObject->yPosition);
        else if (value == "direction") *outValue = Value(spriteObject->rotation);
        else if (value == "costume #") *outValue = Value(spriteObject->currentCostume + 1);
        else if (value == "costume name") *outValue = Value(spriteObject->costumes[spriteObject->currentCostume].name);
        else if (value == "backdrop name") *outValue = Value(spriteObject->costumes[spriteObject->currentCostume].name);
        else if (value == "size") *outValue = Value(spriteObject->size);
        else {
            for (const auto &[id, variable] : spriteObject->variables) {
                if (value == variable.name) *outValue = variable.value;
            }
        }
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, mousex) {
    *outValue = Value(Input::mousePointer.x);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, mousey) {
    *outValue = Value(Input::mousePointer.y);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, distanceto) {
    Value distanceTo;
    if (!Scratch::getInput(block, "DISTANCETOMENU", thread, sprite, distanceTo)) return BlockResult::REPEAT;

    if (distanceTo.asString() == "_mouse_") {
        const double dx = Input::mousePointer.x - sprite->xPosition;
        const double dy = Input::mousePointer.y - sprite->yPosition;
        *outValue = Value(std::sqrt(dx * dx + dy * dy));
        return BlockResult::CONTINUE;
    }

    for (Sprite *currentSprite : Scratch::sprites) {
        if (currentSprite->isClone || currentSprite->name != distanceTo.asString()) continue;
        const double dx = currentSprite->xPosition - sprite->xPosition;
        const double dy = currentSprite->yPosition - sprite->yPosition;
        *outValue = Value(std::sqrt(dx * dx + dy * dy));
        return BlockResult::CONTINUE;
    }
    *outValue = Value(10000);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, dayssince2000) {
    *outValue = Value(Time::getDaysSince2000());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, current) {
    std::string inputValue = Scratch::getFieldValue(*block, "CURRENTMENU");

    if (inputValue == "YEAR") *outValue = Value(Time::getYear());
    else if (inputValue == "MONTH") *outValue = Value(Time::getMonth());
    else if (inputValue == "DATE") *outValue = Value(Time::getDay());
    else if (inputValue == "DAYOFWEEK") *outValue = Value(Time::getDayOfWeek());
    else if (inputValue == "HOUR") *outValue = Value(Time::getHours());
    else if (inputValue == "MINUTE") *outValue = Value(Time::getMinutes());
    else if (inputValue == "SECOND") *outValue = Value(Time::getSeconds());
    else *outValue = Value();
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, answer) {
    *outValue = Value(Scratch::answer);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, keypressed) {
    Value keyOption;
    if (!Scratch::getInput(block, "KEY_OPTION", thread, sprite, keyOption)) return BlockResult::REPEAT;
    *outValue = Value(false);

    for (std::string button : Input::inputButtons) {
        if (Input::convertToKey(keyOption) == button) {
            *outValue = Value(true);
            break;
        }
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, touchingobject) {
    Value touchingObject;
    if (!Scratch::getInput(block, "TOUCHINGOBJECTMENU", thread, sprite, touchingObject)) return BlockResult::REPEAT;

    if (touchingObject.asString() == "_mouse_")
        *outValue = Value(Scratch::isColliding("mouse", sprite));
    else if (touchingObject.asString() == "_edge_")
        *outValue = Value(Scratch::isColliding("edge", sprite));
    else {
        *outValue = Value(false);
        for (size_t i = 0; i < Scratch::sprites.size(); i++) {
            Sprite *currentSprite = Scratch::sprites[i];
            if (currentSprite == sprite) continue;
            if (currentSprite->name == touchingObject.asString() &&
                Scratch::isColliding("sprite", sprite, currentSprite, touchingObject.asString())) {
                *outValue = Value(true);
                return BlockResult::CONTINUE;
            }
        }
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, mousedown) {
    *outValue = Value(Input::mousePointer.isPressed);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, username) {
#ifdef ENABLE_CLOUDVARS
    if (Scratch::cloudProject) *outValue = Value(Scratch::cloudUsername);
    else
#endif
        if (Scratch::useCustomUsername)
        *outValue = Value(Scratch::customUsername);
    else *outValue = Value(OS::getUsername());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sensing, online) {
    *outValue = Value(OS::isOnline());
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