#include "sensing.hpp"
#include "../input.hpp"
#include "../keyboard.hpp"

BlockResult SensingBlocks::resetTimer(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh) {
    BlockExecutor::timer = std::chrono::high_resolution_clock::now();
    return BlockResult::CONTINUE;
}

BlockResult SensingBlocks::askAndWait(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh) {
    Keyboard kbd;
    Value inputValue = Scratch::getInputValue(block, "QUESTION", sprite);
    std::string output = kbd.openKeyboard(inputValue.asString().c_str());
    answer = output;
    return BlockResult::CONTINUE;
}

BlockResult SensingBlocks::setDragMode(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh) {

    std::string mode = block.fields.at("DRAG_MODE")[0];

    if (mode == "draggable") {
        sprite->draggable = true;
    } else if (mode == "not draggable") {
        sprite->draggable = false;
    }

    return BlockResult::CONTINUE;
}

Value SensingBlocks::sensingTimer(Block &block, Sprite *sprite) {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - BlockExecutor::timer).count();
    return Value(elapsed);
}

Value SensingBlocks::of(Block &block, Sprite *sprite) {
    std::string value = block.fields.at("PROPERTY")[0];
    std::string object;
    auto objectFind = block.parsedInputs.find("OBJECT");
    Block *objectBlock = findBlock(objectFind->second.literalValue.asString());
    if (!objectBlock || objectBlock == nullptr)
        return Value();

    object = objectBlock->fields.at("OBJECT")[0];

    Sprite *spriteObject = nullptr;
    for (Sprite *currentSprite : sprites) {
        if (currentSprite->name == object && !currentSprite->isClone) {
            spriteObject = currentSprite;
            break;
        }
    }

    if (!spriteObject) return Value(0);

    if (value == "timer") {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - BlockExecutor::timer).count();
        return Value(elapsed);
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
    auto inputFind = block.parsedInputs.find("DISTANCETOMENU");
    Block *inputBlock = findBlock(inputFind->second.literalValue.asString());
    std::string object = inputBlock->fields.at("DISTANCETOMENU")[0];

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
    return Value(0);
}

Value SensingBlocks::daysSince2000(Block &block, Sprite *sprite) {
    return Value(Time::getDaysSince2000());
}

Value SensingBlocks::current(Block &block, Sprite *sprite) {
    std::string inputValue;
    try {
        inputValue = block.fields.at("CURRENTMENU")[0];
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
    auto inputFind = block.parsedInputs.find("KEY_OPTION");
    std::string buttonCheck;
    
    // if no variable block is in the input
    if(inputFind->second.inputType == ParsedInput::LITERAL){
    Block *inputBlock = findBlock(inputFind->second.literalValue.asString());
    buttonCheck = inputBlock->fields["KEY_OPTION"][0];
    } else{
       buttonCheck = Scratch::getInputValue(block,"KEY_OPTION",sprite).asString();
    }

    for (std::string button : Input::inputButtons) {
        if (buttonCheck == button) {
            return Value(true);
        }
    }

    return Value(false);
}

bool isSeparated(const std::vector<std::pair<double, double>> &poly1,
                 const std::vector<std::pair<double, double>> &poly2,
                 double axisX, double axisY) {
    double min1 = 1e9, max1 = -1e9;
    double min2 = 1e9, max2 = -1e9;

    // Project poly1 onto axis
    for (const auto &point : poly1) {
        double projection = point.first * axisX + point.second * axisY;
        min1 = std::min(min1, projection);
        max1 = std::max(max1, projection);
    }

    // Project poly2 onto axis
    for (const auto &point : poly2) {
        double projection = point.first * axisX + point.second * axisY;
        min2 = std::min(min2, projection);
        max2 = std::max(max2, projection);
    }

    return max1 < min2 || max2 < min1;
}

Value SensingBlocks::touchingObject(Block &block, Sprite *sprite) {
    auto inputFind = block.parsedInputs.find("TOUCHINGOBJECTMENU");
    Block *inputBlock = findBlock(inputFind->second.literalValue.asString());
    std::string objectName;
    try {
        objectName = inputBlock->fields["TOUCHINGOBJECTMENU"][0];
    } catch (...) {
        return Value(false);
    }

    // Get collision points of the current sprite
    std::vector<std::pair<double, double>> currentSpritePoints = getCollisionPoints(sprite);

    if (objectName == "_mouse_") {
        // Define a small square centered on the mouse pointer
        double halfWidth = 0.5;
        double halfHeight = 0.5;

        std::vector<std::pair<double, double>> mousePoints = {
            {Input::mousePointer.x - halfWidth, Input::mousePointer.y - halfHeight}, // Top-left
            {Input::mousePointer.x + halfWidth, Input::mousePointer.y - halfHeight}, // Top-right
            {Input::mousePointer.x + halfWidth, Input::mousePointer.y + halfHeight}, // Bottom-right
            {Input::mousePointer.x - halfWidth, Input::mousePointer.y + halfHeight}  // Bottom-left
        };

        bool collision = true;

        for (int i = 0; i < 4; i++) {
            auto edge1 = std::make_pair(
                currentSpritePoints[(i + 1) % 4].first - currentSpritePoints[i].first,
                currentSpritePoints[(i + 1) % 4].second - currentSpritePoints[i].second);
            auto edge2 = std::make_pair(
                mousePoints[(i + 1) % 4].first - mousePoints[i].first,
                mousePoints[(i + 1) % 4].second - mousePoints[i].second);

            double axis1X = -edge1.second, axis1Y = edge1.first;
            double axis2X = -edge2.second, axis2Y = edge2.first;

            double len1 = sqrt(axis1X * axis1X + axis1Y * axis1Y);
            double len2 = sqrt(axis2X * axis2X + axis2Y * axis2Y);
            if (len1 > 0) {
                axis1X /= len1;
                axis1Y /= len1;
            }
            if (len2 > 0) {
                axis2X /= len2;
                axis2Y /= len2;
            }

            if (isSeparated(currentSpritePoints, mousePoints, axis1X, axis1Y) ||
                isSeparated(currentSpritePoints, mousePoints, axis2X, axis2Y)) {
                collision = false;
                break;
            }
        }

        return Value(collision);
    }

    if (objectName == "_edge_") {
        double halfWidth = Scratch::projectWidth / 2.0;
        double halfHeight = Scratch::projectHeight / 2.0;

        // Check if the current sprite is touching the edge of the screen
        if (sprite->xPosition <= -halfWidth || sprite->xPosition >= halfWidth ||
            sprite->yPosition <= -halfHeight || sprite->yPosition >= halfHeight) {
            return Value(true);
        }
        return Value(false);
    }

    for (Sprite *targetSprite : sprites) {
        if (targetSprite->name == objectName && targetSprite->visible) {
            std::vector<std::pair<double, double>> targetSpritePoints = getCollisionPoints(targetSprite);

            bool collision = true;

            // Check all axes from both rectangles
            for (int i = 0; i < 4; i++) {
                // Get edge vectors for both sprites
                auto edge1 = std::make_pair(
                    currentSpritePoints[(i + 1) % 4].first - currentSpritePoints[i].first,
                    currentSpritePoints[(i + 1) % 4].second - currentSpritePoints[i].second);
                auto edge2 = std::make_pair(
                    targetSpritePoints[(i + 1) % 4].first - targetSpritePoints[i].first,
                    targetSpritePoints[(i + 1) % 4].second - targetSpritePoints[i].second);

                // Get perpendicular axes
                double axis1X = -edge1.second, axis1Y = edge1.first;
                double axis2X = -edge2.second, axis2Y = edge2.first;

                // Normalize axes
                double len1 = sqrt(axis1X * axis1X + axis1Y * axis1Y);
                double len2 = sqrt(axis2X * axis2X + axis2Y * axis2Y);
                if (len1 > 0) {
                    axis1X /= len1;
                    axis1Y /= len1;
                }
                if (len2 > 0) {
                    axis2X /= len2;
                    axis2Y /= len2;
                }

                // Check separation
                if (isSeparated(currentSpritePoints, targetSpritePoints, axis1X, axis1Y) ||
                    isSeparated(currentSpritePoints, targetSpritePoints, axis2X, axis2Y)) {
                    collision = false;
                    break;
                }
            }

            if (collision) {
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