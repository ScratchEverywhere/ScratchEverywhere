#include "motion.hpp"
#include "../render.hpp"
#include "blockExecutor.hpp"
#include "input.hpp"
#include "interpret.hpp"
#include "math.hpp"
#include "render.hpp"
#include "sprite.hpp"
#include "value.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <math.h>
#include <ostream>
#include <string>

BlockResult MotionBlocks::moveSteps(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    const double oldX = sprite->xPosition;
    const double oldY = sprite->yPosition;

    Value value = Scratch::getInputValue(block, "STEPS", sprite);
    if (!value.isNumeric()) return BlockResult::CONTINUE;
    double angle = (sprite->rotation - 90) * M_PI / 180.0;
    sprite->xPosition += std::cos(angle) * value.asDouble();
    sprite->yPosition -= std::sin(angle) * value.asDouble();
    if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

    if (sprite->penData.down && (oldX != sprite->xPosition || oldY != sprite->yPosition)) Render::penMove(oldX, oldY, sprite->xPosition, sprite->yPosition, sprite);

    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::goTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    const double oldX = sprite->xPosition;
    const double oldY = sprite->yPosition;

    auto inputValue = block.parsedInputs->find("TO");
    Block *inputBlock = findBlock(inputValue->second.literalValue.asString());
    std::string objectName = Scratch::getFieldValue(*inputBlock, "TO");

    if (objectName == "_random_") {
        sprite->xPosition = rand() % Scratch::projectWidth - Scratch::projectWidth / 2;
        sprite->yPosition = rand() % Scratch::projectHeight - Scratch::projectHeight / 2;
        goto end;
    }

    if (objectName == "_mouse_") {
        sprite->xPosition = Input::mousePointer.x;
        sprite->yPosition = Input::mousePointer.y;
        goto end;
    }

    for (Sprite *currentSprite : sprites) {
        if (currentSprite->name == objectName) {
            sprite->xPosition = currentSprite->xPosition;
            sprite->yPosition = currentSprite->yPosition;
            break;
        }
    }
    if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

end:
    if (sprite->penData.down && (oldX != sprite->xPosition || oldY != sprite->yPosition)) Render::penMove(oldX, oldY, sprite->xPosition, sprite->yPosition, sprite);
    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::goToXY(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    const double oldX = sprite->xPosition;
    const double oldY = sprite->yPosition;

    Value xVal = Scratch::getInputValue(block, "X", sprite);
    Value yVal = Scratch::getInputValue(block, "Y", sprite);
    if (xVal.isNumeric()) sprite->xPosition = xVal.asDouble();
    if (yVal.isNumeric()) sprite->yPosition = yVal.asDouble();
    if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

    if (sprite->penData.down && (oldX != sprite->xPosition || oldY != sprite->yPosition)) Render::penMove(oldX, oldY, sprite->xPosition, sprite->yPosition, sprite);

    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::turnLeft(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value value = Scratch::getInputValue(block, "DEGREES", sprite);
    if (value.isNumeric()) {
        sprite->rotation -= value.asDouble();
    }
    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::turnRight(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value value = Scratch::getInputValue(block, "DEGREES", sprite);
    if (value.isNumeric()) {
        sprite->rotation += value.asDouble();
    }
    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::pointInDirection(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value value = Scratch::getInputValue(block, "DIRECTION", sprite);
    if (value.isNumeric()) {
        sprite->rotation = value.asDouble();
    }
    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::changeXBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    const double oldX = sprite->xPosition;

    Value value = Scratch::getInputValue(block, "DX", sprite);
    if (!value.isNumeric()) return BlockResult::CONTINUE;

    sprite->xPosition += value.asDouble();
    if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

    if (sprite->penData.down && oldX != sprite->xPosition) Render::penMove(oldX, sprite->yPosition, sprite->xPosition, sprite->yPosition, sprite);

    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::changeYBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    const double oldY = sprite->yPosition;

    Value value = Scratch::getInputValue(block, "DY", sprite);
    if (!value.isNumeric()) return BlockResult::CONTINUE;
    sprite->yPosition += value.asDouble();
    if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

    if (sprite->penData.down && oldY != sprite->yPosition) Render::penMove(sprite->xPosition, oldY, sprite->xPosition, sprite->yPosition, sprite);

    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::setX(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    const double oldX = sprite->xPosition;

    Value value = Scratch::getInputValue(block, "X", sprite);
    if (!value.isNumeric()) return BlockResult::CONTINUE;
    sprite->xPosition = value.asDouble();
    if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

    if (sprite->penData.down && oldX != sprite->xPosition) Render::penMove(oldX, sprite->yPosition, sprite->xPosition, sprite->yPosition, sprite);

    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::setY(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    const double oldY = sprite->yPosition;

    Value value = Scratch::getInputValue(block, "Y", sprite);
    if (!value.isNumeric()) return BlockResult::CONTINUE;
    sprite->yPosition = value.asDouble();
    if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

    if (sprite->penData.down && oldY != sprite->yPosition) Render::penMove(sprite->xPosition, oldY, sprite->xPosition, sprite->yPosition, sprite);

    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::glideSecsToXY(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (block.repeatTimes != -1 && !fromRepeat) {
        block.repeatTimes = -1;
    }

    if (block.repeatTimes == -1) {
        block.repeatTimes = -6;

        Value duration = Scratch::getInputValue(block, "SECS", sprite);
        if (duration.isNumeric()) {
            block.waitDuration = duration.asDouble() * 1000; // milliseconds
        } else {
            block.waitDuration = 0;
        }

        block.waitTimer.start();
        block.glideStartX = sprite->xPosition;
        block.glideStartY = sprite->yPosition;

        // Get target positions
        Value positionXStr = Scratch::getInputValue(block, "X", sprite);
        Value positionYStr = Scratch::getInputValue(block, "Y", sprite);
        block.glideEndX = positionXStr.isNumeric() ? positionXStr.asDouble() : block.glideStartX;
        block.glideEndY = positionYStr.isNumeric() ? positionYStr.asDouble() : block.glideStartY;

        BlockExecutor::addToRepeatQueue(sprite, const_cast<Block *>(&block));
    }

    int elapsedTime = block.waitTimer.getTimeMs();

    if (elapsedTime >= block.waitDuration) {
        sprite->xPosition = block.glideEndX;
        sprite->yPosition = block.glideEndY;
        if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

        block.repeatTimes = -1;
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    double progress = static_cast<double>(elapsedTime) / block.waitDuration;
    if (progress > 1.0) progress = 1.0;

    const double oldX = sprite->xPosition;
    const double oldY = sprite->yPosition;

    sprite->xPosition = block.glideStartX + (block.glideEndX - block.glideStartX) * progress;
    sprite->yPosition = block.glideStartY + (block.glideEndY - block.glideStartY) * progress;
    if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

    if (sprite->penData.down && (oldX != sprite->xPosition || oldY != sprite->yPosition)) Render::penMove(oldX, oldY, sprite->xPosition, sprite->yPosition, sprite);

    return BlockResult::RETURN;
}

BlockResult MotionBlocks::glideTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {

    if (block.repeatTimes != -1 && !fromRepeat) {
        block.repeatTimes = -1;
    }

    if (block.repeatTimes == -1) {
        block.repeatTimes = -7;

        Value duration = Scratch::getInputValue(block, "SECS", sprite);
        if (duration.isNumeric()) {
            block.waitDuration = duration.asDouble() * 1000; // Convert to milliseconds
        } else {
            block.waitDuration = 0;
        }

        block.waitTimer.start();
        block.glideStartX = sprite->xPosition;
        block.glideStartY = sprite->yPosition;

        Block *inputBlock;
        auto itVal = block.parsedInputs->find("TO");
        inputBlock = findBlock(itVal->second.literalValue.asString());
        if (!inputBlock) return BlockResult::CONTINUE;

        std::string inputValue = Scratch::getFieldValue(*inputBlock, "TO");
        std::string positionXStr;
        std::string positionYStr;

        if (inputValue == "_random_") {
            positionXStr = std::to_string(rand() % Scratch::projectWidth - Scratch::projectWidth / 2);
            positionYStr = std::to_string(rand() % Scratch::projectHeight - Scratch::projectHeight / 2);
        } else if (inputValue == "_mouse_") {
            positionXStr = std::to_string(Input::mousePointer.x);
            positionYStr = std::to_string(Input::mousePointer.y);
        } else {
            for (auto &currentSprite : sprites) {
                if (currentSprite->name == inputValue) {
                    positionXStr = std::to_string(currentSprite->xPosition);
                    positionYStr = std::to_string(currentSprite->yPosition);
                    break;
                }
            }
        }

        block.glideEndX = Math::isNumber(positionXStr) ? std::stod(positionXStr) : block.glideStartX;
        block.glideEndY = Math::isNumber(positionYStr) ? std::stod(positionYStr) : block.glideStartY;

        BlockExecutor::addToRepeatQueue(sprite, const_cast<Block *>(&block));
    }

    int elapsedTime = block.waitTimer.getTimeMs();

    if (elapsedTime >= block.waitDuration) {
        sprite->xPosition = block.glideEndX;
        sprite->yPosition = block.glideEndY;
        if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

        block.repeatTimes = -1;
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    double progress = static_cast<double>(elapsedTime) / block.waitDuration;
    if (progress > 1.0) progress = 1.0;

    const double oldX = sprite->xPosition;
    const double oldY = sprite->yPosition;

    sprite->xPosition = block.glideStartX + (block.glideEndX - block.glideStartX) * progress;
    sprite->yPosition = block.glideStartY + (block.glideEndY - block.glideStartY) * progress;
    if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

    if (sprite->penData.down && (oldX != sprite->xPosition || oldY != sprite->yPosition)) Render::penMove(oldX, oldY, sprite->xPosition, sprite->yPosition, sprite);

    return BlockResult::RETURN;
}

BlockResult MotionBlocks::pointToward(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    auto itVal = block.parsedInputs->find("TOWARDS");
    Block *inputBlock = findBlock(itVal->second.literalValue.asString());

    std::string objectName = Scratch::getFieldValue(*inputBlock, "TOWARDS");
    double targetX = 0;
    double targetY = 0;

    if (objectName == "_random_") {
        sprite->rotation = rand() % 360;
        return BlockResult::CONTINUE;
    }

    if (objectName == "_mouse_") {
        targetX = Input::mousePointer.x;
        targetY = Input::mousePointer.y;
    } else {
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->name == objectName) {
                targetX = currentSprite->xPosition;
                targetY = currentSprite->yPosition;
                break;
            }
        }
    }

    const double dx = targetX - sprite->xPosition;
    const double dy = targetY - sprite->yPosition;
    double angle = 90 - (atan2(dy, dx) * 180.0 / M_PI);
    sprite->rotation = angle;
    // std::cout << "Pointing towards " << sprite->rotation << std::endl;
    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::setRotationStyle(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::string value;
    try {
        value = Scratch::getFieldValue(block, "STYLE");
        ;
    } catch (...) {
        std::cerr << "unable to find rotation style." << std::endl;
        return BlockResult::CONTINUE;
    }

    if (value == "left-right") {
        sprite->rotationStyle = sprite->LEFT_RIGHT;
    } else if (value == "don't rotate") {
        sprite->rotationStyle = sprite->NONE;
    } else {
        sprite->rotationStyle = sprite->ALL_AROUND;
    }
    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::ifOnEdgeBounce(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    double halfWidth = Scratch::projectWidth / 2.0;
    double halfHeight = Scratch::projectHeight / 2.0;

    double scale = sprite->size / 100.0;
    double spriteHalfWidth = (sprite->spriteWidth * scale) / 2.0;
    double spriteHalfHeight = (sprite->spriteHeight * scale) / 2.0;

    // Compute bounds of the sprite
    double left = sprite->xPosition - spriteHalfWidth;
    double right = sprite->xPosition + spriteHalfWidth;
    double top = sprite->yPosition + spriteHalfHeight;
    double bottom = sprite->yPosition - spriteHalfHeight;

    // Compute distances from edges (positive when far from edge, zero or negative when overlapping)
    double distLeft = std::max(0.0, halfWidth + left);
    double distRight = std::max(0.0, halfWidth - right);
    double distTop = std::max(0.0, halfHeight - top);
    double distBottom = std::max(0.0, halfHeight + bottom);

    // Determine the nearest edge being touched
    std::string nearestEdge = "";
    double minDist = INFINITY;

    if (distLeft < minDist) {
        minDist = distLeft;
        nearestEdge = "left";
    }
    if (distTop < minDist) {
        minDist = distTop;
        nearestEdge = "top";
    }
    if (distRight < minDist) {
        minDist = distRight;
        nearestEdge = "right";
    }
    if (distBottom < minDist) {
        minDist = distBottom;
        nearestEdge = "bottom";
    }

    // Not touching any edge
    if (minDist > 0.0) {
        return BlockResult::CONTINUE;
    }

    // Convert current direction to radians
    double radians = (90.0 - sprite->rotation) * (M_PI / 180.0);
    double dx = std::cos(radians);
    double dy = -std::sin(radians);

    // Reflect the direction based on the edge hit
    if (nearestEdge == "left") {
        dx = std::max(0.2, std::abs(dx));
    } else if (nearestEdge == "right") {
        dx = -std::max(0.2, std::abs(dx));
    } else if (nearestEdge == "top") {
        dy = std::max(0.2, std::abs(dy));
    } else if (nearestEdge == "bottom") {
        dy = -std::max(0.2, std::abs(dy));
    }

    // Calculate new direction from reflected vector
    sprite->rotation = std::atan2(dy, dx) * (180.0 / M_PI) + 90.0;

    // Clamp sprite back into stage bounds
    double dxCorrection = 0;
    double dyCorrection = 0;

    if (left < -halfWidth) dxCorrection += -halfWidth - left;
    if (right > halfWidth) dxCorrection += halfWidth - right;
    if (top > halfHeight) dyCorrection += halfHeight - top;
    if (bottom < -halfHeight) dyCorrection += -halfHeight - bottom;

    const double oldX = sprite->xPosition;
    const double oldY = sprite->yPosition;

    sprite->xPosition += dxCorrection;
    sprite->yPosition += dyCorrection;

    if (sprite->penData.down && (oldX != sprite->xPosition || oldY != sprite->yPosition)) Render::penMove(oldX, oldY, sprite->xPosition, sprite->yPosition, sprite);

    return BlockResult::CONTINUE;
}

Value MotionBlocks::xPosition(Block &block, Sprite *sprite) {
    return Value(sprite->xPosition);
}

Value MotionBlocks::yPosition(Block &block, Sprite *sprite) {
    return Value(sprite->yPosition);
}

Value MotionBlocks::direction(Block &block, Sprite *sprite) {
    return Value(sprite->rotation);
}
