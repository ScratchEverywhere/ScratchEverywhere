#include "blockUtils.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <input.hpp>
#include <math.h>
#include <math.hpp>
#include <render.hpp>
#include <sprite.hpp>
#include <string>
#include <value.hpp>

SCRATCH_BLOCK(motion, movesteps) {
    const double value = Scratch::getInputValue(block, "STEPS", sprite).asDouble();
    const double angle = Math::degreesToRadians(90 - sprite->rotation);
    Scratch::gotoXY(sprite, sprite->xPosition + std::cos(angle) * value, sprite->yPosition + std::sin(angle) * value);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, goto) {
    const std::string objectName = Scratch::getInputValue(block, "TO", sprite).asString();

    if (objectName == "_random_") {
        Scratch::gotoXY(sprite, rand() % Scratch::projectWidth - Scratch::projectWidth / 2, rand() % Scratch::projectHeight - Scratch::projectHeight / 2);
    } else if (objectName == "_mouse_") {
        Scratch::gotoXY(sprite, Input::mousePointer.x, Input::mousePointer.y);
    } else {
        for (Sprite *currentSprite : Scratch::sprites) {
            if (currentSprite->name == objectName) {
                Scratch::gotoXY(sprite, currentSprite->xPosition, currentSprite->yPosition);
                break;
            }
        }
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, gotoxy) {
    const double xVal = Scratch::getInputValue(block, "X", sprite).asDouble();
    const double yVal = Scratch::getInputValue(block, "Y", sprite).asDouble();
    Scratch::gotoXY(sprite, xVal, yVal);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, turnleft) {
    const double direction = Scratch::getInputValue(block, "DEGREES", sprite).asDouble();
    Scratch::setDirection(sprite, sprite->rotation - direction);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, turnright) {
    const double direction = Scratch::getInputValue(block, "DEGREES", sprite).asDouble();
    Scratch::setDirection(sprite, sprite->rotation + direction);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, pointindirection) {
    const double direction = Scratch::getInputValue(block, "DIRECTION", sprite).asDouble();
    Scratch::setDirection(sprite, direction);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, changexby) {
    const double dx = Scratch::getInputValue(block, "DX", sprite).asDouble();
    Scratch::gotoXY(sprite, sprite->xPosition + dx, sprite->yPosition);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, changeyby) {
    const double dy = Scratch::getInputValue(block, "DY", sprite).asDouble();
    Scratch::gotoXY(sprite, sprite->xPosition, sprite->yPosition + dy);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, setx) {
    const double X = Scratch::getInputValue(block, "X", sprite).asDouble();
    Scratch::gotoXY(sprite, X, sprite->yPosition);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, sety) {
    const double Y = Scratch::getInputValue(block, "Y", sprite).asDouble();
    Scratch::gotoXY(sprite, sprite->xPosition, Y);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, glidesecstoxy) {
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

    const int elapsedTime = block.waitTimer.getTimeMs();

    if (elapsedTime >= block.waitDuration) {
        Scratch::gotoXY(sprite, block.glideEndX, block.glideEndY);

        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    double progress = static_cast<double>(elapsedTime) / block.waitDuration;
    Scratch::gotoXY(sprite, block.glideStartX + (block.glideEndX - block.glideStartX) * progress, block.glideStartY + (block.glideEndY - block.glideStartY) * progress);

    return BlockResult::RETURN;
}

SCRATCH_BLOCK(motion, glideto) {
    if (!fromRepeat) {
        double duration = Scratch::getInputValue(block, "SECS", sprite).asDouble();
        block.waitDuration = duration * 1000; // Convert to milliseconds

        block.waitTimer.start();
        block.glideStartX = sprite->xPosition;
        block.glideStartY = sprite->yPosition;

        const std::string inputValue = Scratch::getInputValue(block, "TO", sprite).asString();
        double positionXStr = sprite->xPosition;
        double positionYStr = sprite->yPosition;

        if (inputValue == "_random_") {
            positionXStr = rand() % Scratch::projectWidth - Scratch::projectWidth / 2;
            positionYStr = rand() % Scratch::projectHeight - Scratch::projectHeight / 2;
        } else if (inputValue == "_mouse_") {
            positionXStr = Input::mousePointer.x;
            positionYStr = Input::mousePointer.y;
        } else {
            for (auto &currentSprite : Scratch::sprites) {
                if (currentSprite->name != inputValue) continue;
                positionXStr = currentSprite->xPosition;
                positionYStr = currentSprite->yPosition;
                break;
            }
        }

        block.glideEndX = positionXStr;
        block.glideEndY = positionYStr;

        BlockExecutor::addToRepeatQueue(sprite, const_cast<Block *>(&block));
    }

    const int elapsedTime = block.waitTimer.getTimeMs();

    if (elapsedTime >= block.waitDuration) {
        Scratch::gotoXY(sprite, block.glideEndX, block.glideEndY);

        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    const double progress = static_cast<double>(elapsedTime) / block.waitDuration;
    Scratch::gotoXY(sprite, block.glideStartX + (block.glideEndX - block.glideStartX) * progress, block.glideStartY + (block.glideEndY - block.glideStartY) * progress);

    return BlockResult::RETURN;
}

SCRATCH_BLOCK(motion, pointtowards) {
    const std::string objectName = Scratch::getInputValue(block, "TOWARDS", sprite).asString();
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
        for (Sprite *currentSprite : Scratch::sprites) {
            if (currentSprite->name == objectName) {
                targetX = currentSprite->xPosition;
                targetY = currentSprite->yPosition;
                break;
            }
        }
    }

    const double dx = targetX - sprite->xPosition;
    const double dy = targetY - sprite->yPosition;

    const double angle = 90 - Math::radiansToDegrees(atan2(dy, dx));
    Scratch::setDirection(sprite, angle);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, setrotationstyle) {
    const std::string value = Scratch::getFieldValue(block, "STYLE");

    if (value == "left-right") {
        sprite->rotationStyle = sprite->LEFT_RIGHT;
        return BlockResult::CONTINUE;
    }
    if (value == "don't rotate") {
        sprite->rotationStyle = sprite->NONE;
        return BlockResult::CONTINUE;
    }
    sprite->rotationStyle = sprite->ALL_AROUND;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, ifonedgebounce) {
    const double halfWidth = Scratch::projectWidth / 2.0;
    const double halfHeight = Scratch::projectHeight / 2.0;

    const double scale = sprite->size / 100.0;
    const double spriteHalfWidth = (sprite->spriteWidth * scale) / 2.0;
    const double spriteHalfHeight = (sprite->spriteHeight * scale) / 2.0;

    // Compute bounds of the sprite
    const double left = sprite->xPosition - spriteHalfWidth;
    const double right = sprite->xPosition + spriteHalfWidth;
    const double top = sprite->yPosition + spriteHalfHeight;
    const double bottom = sprite->yPosition - spriteHalfHeight;

    // Compute distances from edges (positive when far from edge, zero or negative when overlapping)
    const double distLeft = std::max(0.0, halfWidth + left);
    const double distRight = std::max(0.0, halfWidth - right);
    const double distTop = std::max(0.0, halfHeight - top);
    const double distBottom = std::max(0.0, halfHeight + bottom);

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

    if (!Scratch::isColliding("edge", sprite))
        return BlockResult::CONTINUE;

    // Convert current direction to radians
    const double radians = Math::degreesToRadians(90.0 - sprite->rotation);
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

SCRATCH_REPORTER_BLOCK(motion, xposition) {
    double rounded = std::round(sprite->xPosition);
    double delta = std::fabs(sprite->xPosition - rounded);
    return Value((delta < 1e-9) ? rounded : sprite->xPosition);
}

SCRATCH_REPORTER_BLOCK(motion, yposition) {
    double rounded = std::round(sprite->yPosition);
    double delta = std::fabs(sprite->yPosition - rounded);
    return Value((delta < 1e-9) ? rounded : sprite->yPosition);
}

SCRATCH_REPORTER_BLOCK(motion, direction) {
    return Value(sprite->rotation);
}
