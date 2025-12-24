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
    double value = Scratch::getInputValue(block, "STEPS", sprite).asDouble();
    double angle = Math::degreesToRadians(90 - sprite->rotation);
    Scratch::gotoXY(sprite, sprite->xPosition + std::cos(angle) * value, sprite->yPosition + std::sin(angle) * value);

    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::goTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::string objectName = Scratch::getInputValue(block, "TO", sprite).asString();

    if (objectName == "_random_") {
        Scratch::gotoXY(sprite, rand() % Scratch::projectWidth - Scratch::projectWidth / 2, rand() % Scratch::projectHeight - Scratch::projectHeight / 2);
    } else if (objectName == "_mouse_") {
        Scratch::gotoXY(sprite, Input::mousePointer.x, Input::mousePointer.y);
    } else {
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->name == objectName) {
                Scratch::gotoXY(sprite, currentSprite->xPosition, currentSprite->yPosition);
                break;
            }
        }
    }

    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::goToXY(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    double xVal = Scratch::getInputValue(block, "X", sprite).asDouble();
    double yVal = Scratch::getInputValue(block, "Y", sprite).asDouble();
    Scratch::gotoXY(sprite, xVal, yVal);

    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::turnLeft(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    double direction = Scratch::getInputValue(block, "DEGREES", sprite).asDouble();
    Scratch::setDirection(sprite, sprite->rotation - direction);
    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::turnRight(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    double direction = Scratch::getInputValue(block, "DEGREES", sprite).asDouble();
    Scratch::setDirection(sprite, sprite->rotation + direction);
    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::pointInDirection(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    double direction = Scratch::getInputValue(block, "DIRECTION", sprite).asDouble();
    Scratch::setDirection(sprite, direction);
    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::changeXBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    double dx = Scratch::getInputValue(block, "DX", sprite).asDouble();
    Scratch::gotoXY(sprite, sprite->xPosition + dx, sprite->yPosition);

    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::changeYBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    double dy = Scratch::getInputValue(block, "DY", sprite).asDouble();
    Scratch::gotoXY(sprite, sprite->xPosition, sprite->yPosition + dy);

    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::setX(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    double X = Scratch::getInputValue(block, "X", sprite).asDouble();
    Scratch::gotoXY(sprite, X, sprite->yPosition);

    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::setY(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    double Y = Scratch::getInputValue(block, "Y", sprite).asDouble();
    Scratch::gotoXY(sprite, sprite->xPosition, Y);

    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::glideSecsToXY(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!fromRepeat) {

        double duration = Scratch::getInputValue(block, "SECS", sprite).asDouble();
        block.waitDuration = duration * 1000; // milliseconds

        block.waitTimer.start();
        block.glideStartX = sprite->xPosition;
        block.glideStartY = sprite->yPosition;

        // Get target positions
        block.glideEndX = Scratch::getInputValue(block, "X", sprite).asDouble();
        block.glideEndY = Scratch::getInputValue(block, "Y", sprite).asDouble();

        BlockExecutor::addToRepeatQueue(sprite, const_cast<Block *>(&block));
    }

    int elapsedTime = block.waitTimer.getTimeMs();

    if (elapsedTime >= block.waitDuration) {
        Scratch::gotoXY(sprite, block.glideEndX, block.glideEndY);

        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    double progress = static_cast<double>(elapsedTime) / block.waitDuration;
    Scratch::gotoXY(sprite, block.glideStartX + (block.glideEndX - block.glideStartX) * progress, block.glideStartY + (block.glideEndY - block.glideStartY) * progress);

    return BlockResult::RETURN;
}

BlockResult MotionBlocks::glideTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!fromRepeat) {
        double duration = Scratch::getInputValue(block, "SECS", sprite).asDouble();
        block.waitDuration = duration * 1000; // Convert to milliseconds

        block.waitTimer.start();
        block.glideStartX = sprite->xPosition;
        block.glideStartY = sprite->yPosition;

        std::string inputValue = Scratch::getInputValue(block, "TO", sprite).asString();
        double positionXStr = sprite->xPosition;
        double positionYStr = sprite->yPosition;

        if (inputValue == "_random_") {
            positionXStr = rand() % Scratch::projectWidth - Scratch::projectWidth / 2;
            positionYStr = rand() % Scratch::projectHeight - Scratch::projectHeight / 2;
        } else if (inputValue == "_mouse_") {
            positionXStr = Input::mousePointer.x;
            positionYStr = Input::mousePointer.y;
        } else {
            for (auto &currentSprite : sprites) {
                if (currentSprite->name == inputValue) {
                    positionXStr = currentSprite->xPosition;
                    positionYStr = currentSprite->yPosition;
                    break;
                }
            }
        }

        block.glideEndX = positionXStr;
        block.glideEndY = positionYStr;

        BlockExecutor::addToRepeatQueue(sprite, const_cast<Block *>(&block));
    }

    int elapsedTime = block.waitTimer.getTimeMs();

    if (elapsedTime >= block.waitDuration) {
        Scratch::gotoXY(sprite, block.glideEndX, block.glideEndY);

        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    double progress = static_cast<double>(elapsedTime) / block.waitDuration;
    Scratch::gotoXY(sprite, block.glideStartX + (block.glideEndX - block.glideStartX) * progress, block.glideStartY + (block.glideEndY - block.glideStartY) * progress);

    return BlockResult::RETURN;
}

BlockResult MotionBlocks::pointToward(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::string objectName = Scratch::getInputValue(block, "TOWARDS", sprite).asString();
    double targetX = 0;
    double targetY = 0;

    if (objectName == "_random_") {
        sprite->rotation = (rand() % 360) - 179.0f;
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
    double angle = 90 - Math::radiansToDegrees(atan2(dy, dx));
    Scratch::setDirection(sprite, angle);
    return BlockResult::CONTINUE;
}

BlockResult MotionBlocks::setRotationStyle(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::string value = Scratch::getFieldValue(block, "STYLE");

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

    if (!isColliding("edge", sprite))
        return BlockResult::CONTINUE;

    // Convert current direction to radians
    double radians = Math::degreesToRadians(90.0 - sprite->rotation);
    double dx = std::cos(radians);
    double dy = -std::sin(radians);

    // Reflect the direction based on the edge hit
    if (nearestEdge == "left") {
        dx = std::max(0.2, std::fabs(dx));
    } else if (nearestEdge == "right") {
        dx = -std::max(0.2, std::fabs(dx));
    } else if (nearestEdge == "top") {
        dy = std::max(0.2, std::fabs(dy));
    } else if (nearestEdge == "bottom") {
        dy = -std::max(0.2, std::fabs(dy));
    }

    // Calculate new direction from reflected vector
    sprite->rotation = Math::radiansToDegrees(atan2(dy, dx)) + 90.0;

    // Clamp sprite back into stage bounds
    double dxCorrection = 0;
    double dyCorrection = 0;

    if (left < -halfWidth) dxCorrection += -halfWidth - left;
    if (right > halfWidth) dxCorrection += halfWidth - right;
    if (top > halfHeight) dyCorrection += halfHeight - top;
    if (bottom < -halfHeight) dyCorrection += -halfHeight - bottom;

    Scratch::gotoXY(sprite, sprite->xPosition + dxCorrection, sprite->yPosition + dyCorrection);
    return BlockResult::CONTINUE;
}

Value MotionBlocks::xPosition(Block &block, Sprite *sprite) {
    double rounded = std::round(sprite->xPosition);
    double delta = std::fabs(sprite->xPosition - rounded);
    return Value((delta < 1e-9) ? rounded : sprite->xPosition);
}

Value MotionBlocks::yPosition(Block &block, Sprite *sprite) {
    double rounded = std::round(sprite->yPosition);
    double delta = std::fabs(sprite->yPosition - rounded);
    return Value((delta < 1e-9) ? rounded : sprite->yPosition);
}

Value MotionBlocks::direction(Block &block, Sprite *sprite) {
    return Value(sprite->rotation);
}
