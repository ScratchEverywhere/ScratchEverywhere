#include "../render.hpp"
#include "blockUtils.hpp"
#include "input.hpp"
#include "interpret.hpp"
#include "math.hpp"
#include "render.hpp"
#include "sprite.hpp"
#include "value.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <math.h>
#include <string>

SCRATCH_BLOCK(motion, movesteps) {
    const double oldX = sprite->xPosition;
    const double oldY = sprite->yPosition;

    Value value = Scratch::getInputValue(block, "STEPS", sprite);
    if (!value.isNumeric()) return BlockResult::CONTINUE;
    double angle = (sprite->rotation - 90) * M_PI / 180.0;
    sprite->xPosition += std::cos(angle) * value.asDouble();
    sprite->yPosition -= std::sin(angle) * value.asDouble();
    if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

    if (sprite->penData.down && (oldX != sprite->xPosition || oldY != sprite->yPosition)) Render::penMove(oldX, oldY, sprite->xPosition, sprite->yPosition, sprite);
    Scratch::forceRedraw = true;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, goto) {
    const Value objectName = Scratch::getInputValue(block, "TO", sprite);

    const double oldX = sprite->xPosition;
    const double oldY = sprite->yPosition;

    if (objectName.asString() == "_random_") {
        sprite->xPosition = rand() % Scratch::projectWidth - Scratch::projectWidth / 2;
        sprite->yPosition = rand() % Scratch::projectHeight - Scratch::projectHeight / 2;
        goto end;
    }

    if (objectName.asString() == "_mouse_") {
        sprite->xPosition = Input::mousePointer.x;
        sprite->yPosition = Input::mousePointer.y;
        goto end;
    }

    for (Sprite *currentSprite : sprites) {
        if (currentSprite->name == objectName.asString()) {
            sprite->xPosition = currentSprite->xPosition;
            sprite->yPosition = currentSprite->yPosition;
            break;
        }
    }
    if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

end:
    if (sprite->penData.down && (oldX != sprite->xPosition || oldY != sprite->yPosition)) Render::penMove(oldX, oldY, sprite->xPosition, sprite->yPosition, sprite);
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, gotoxy) {
    const double oldX = sprite->xPosition;
    const double oldY = sprite->yPosition;

    const Value xVal = Scratch::getInputValue(block, "X", sprite);
    const Value yVal = Scratch::getInputValue(block, "Y", sprite);
    if (xVal.isNumeric()) sprite->xPosition = xVal.asDouble();
    if (yVal.isNumeric()) sprite->yPosition = yVal.asDouble();
    if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

    if (sprite->penData.down && (oldX != sprite->xPosition || oldY != sprite->yPosition)) Render::penMove(oldX, oldY, sprite->xPosition, sprite->yPosition, sprite);
    Scratch::forceRedraw = true;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, turnleft) {
    const Value value = Scratch::getInputValue(block, "DEGREES", sprite);
    const double direction = value.asDouble();
    if (direction == std::numeric_limits<double>::infinity() || direction == -std::numeric_limits<double>::infinity()) return BlockResult::CONTINUE;
    sprite->rotation -= direction - floor((direction + 179) / 360) * 360;
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, turnright) {
    const Value value = Scratch::getInputValue(block, "DEGREES", sprite);
    const double direction = value.asDouble();
    if (direction == std::numeric_limits<double>::infinity() || direction == -std::numeric_limits<double>::infinity()) return BlockResult::CONTINUE;
    sprite->rotation += direction - floor((direction + 179) / 360) * 360;
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, pointindirection) {
    const Value value = Scratch::getInputValue(block, "DIRECTION", sprite);
    const double direction = value.asDouble();
    if (direction == std::numeric_limits<double>::infinity() || direction == -std::numeric_limits<double>::infinity()) return BlockResult::CONTINUE;
    sprite->rotation = direction - floor((direction + 179) / 360) * 360;
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, changexby) {
    const double oldX = sprite->xPosition;

    const Value value = Scratch::getInputValue(block, "DX", sprite);
    if (!value.isNumeric()) return BlockResult::CONTINUE;

    sprite->xPosition += value.asDouble();
    if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

    if (sprite->penData.down && oldX != sprite->xPosition) Render::penMove(oldX, sprite->yPosition, sprite->xPosition, sprite->yPosition, sprite);
    Scratch::forceRedraw = true;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, changeyby) {
    const double oldY = sprite->yPosition;

    const Value value = Scratch::getInputValue(block, "DY", sprite);
    if (!value.isNumeric()) return BlockResult::CONTINUE;

    sprite->yPosition += value.asDouble();
    if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

    if (sprite->penData.down && oldY != sprite->yPosition) Render::penMove(sprite->xPosition, oldY, sprite->xPosition, sprite->yPosition, sprite);
    Scratch::forceRedraw = true;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, setx) {
    const double oldX = sprite->xPosition;

    const Value value = Scratch::getInputValue(block, "X", sprite);
    if (!value.isNumeric()) return BlockResult::CONTINUE;

    sprite->xPosition = value.asDouble();
    if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

    if (sprite->penData.down && oldX != sprite->xPosition) Render::penMove(oldX, sprite->yPosition, sprite->xPosition, sprite->yPosition, sprite);
    Scratch::forceRedraw = true;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, sety) {
    const double oldY = sprite->yPosition;

    const Value value = Scratch::getInputValue(block, "Y", sprite);
    if (!value.isNumeric()) return BlockResult::CONTINUE;

    sprite->yPosition = value.asDouble();
    if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

    if (sprite->penData.down && oldY != sprite->yPosition) Render::penMove(sprite->xPosition, oldY, sprite->xPosition, sprite->yPosition, sprite);
    Scratch::forceRedraw = true;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, glidesecstoxy) {
    if (block.repeatTimes != -1 && !fromRepeat) block.repeatTimes = -1;

    if (block.repeatTimes == -1) {
        block.repeatTimes = -6;

        const Value duration = Scratch::getInputValue(block, "SECS", sprite);
        block.waitDuration = duration.isNumeric() ? duration.asDouble() * 1000 : 0;

        block.waitTimer.start();
        block.glideStartX = sprite->xPosition;
        block.glideStartY = sprite->yPosition;

        // Get target positions
        const Value positionXStr = Scratch::getInputValue(block, "X", sprite);
        const Value positionYStr = Scratch::getInputValue(block, "Y", sprite);
        block.glideEndX = positionXStr.isNumeric() ? positionXStr.asDouble() : block.glideStartX;
        block.glideEndY = positionYStr.isNumeric() ? positionYStr.asDouble() : block.glideStartY;

        BlockExecutor::addToRepeatQueue(sprite, const_cast<Block *>(&block));
    }

    const int elapsedTime = block.waitTimer.getTimeMs();

    if (elapsedTime >= block.waitDuration) {
        sprite->xPosition = block.glideEndX;
        sprite->yPosition = block.glideEndY;
        if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);
        Scratch::forceRedraw = true;

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
    Scratch::forceRedraw = true;

    return BlockResult::RETURN;
}

SCRATCH_BLOCK(motion, glideto) {
    if (block.repeatTimes != -1 && !fromRepeat) block.repeatTimes = -1;

    if (block.repeatTimes == -1) {
        block.repeatTimes = -7;

        const Value duration = Scratch::getInputValue(block, "SECS", sprite);
        block.waitDuration = duration.isNumeric() ? duration.asDouble() * 1000 : 0;

        block.waitTimer.start();
        block.glideStartX = sprite->xPosition;
        block.glideStartY = sprite->yPosition;

        const Value inputValue = Scratch::getInputValue(block, "TO", sprite);
        std::string positionXStr;
        std::string positionYStr;

        if (inputValue.asString() == "_random_") {
            positionXStr = std::to_string(rand() % Scratch::projectWidth - Scratch::projectWidth / 2);
            positionYStr = std::to_string(rand() % Scratch::projectHeight - Scratch::projectHeight / 2);
        } else if (inputValue.asString() == "_mouse_") {
            positionXStr = std::to_string(Input::mousePointer.x);
            positionYStr = std::to_string(Input::mousePointer.y);
        } else {
            for (auto &currentSprite : sprites) {
                if (currentSprite->name != inputValue.asString()) continue;
                positionXStr = std::to_string(currentSprite->xPosition);
                positionYStr = std::to_string(currentSprite->yPosition);
                break;
            }
        }

        block.glideEndX = Math::isNumber(positionXStr) ? std::stod(positionXStr) : block.glideStartX;
        block.glideEndY = Math::isNumber(positionYStr) ? std::stod(positionYStr) : block.glideStartY;

        BlockExecutor::addToRepeatQueue(sprite, const_cast<Block *>(&block));
    }

    const int elapsedTime = block.waitTimer.getTimeMs();

    if (elapsedTime >= block.waitDuration) {
        sprite->xPosition = block.glideEndX;
        sprite->yPosition = block.glideEndY;
        if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);
        Scratch::forceRedraw = true;

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
    Scratch::forceRedraw = true;

    return BlockResult::RETURN;
}

SCRATCH_BLOCK(motion, pointtoward) {
    const Value objectName = Scratch::getInputValue(block, "TOWARDS", sprite);
    double targetX = 0;
    double targetY = 0;

    if (objectName.asString() == "_random_") {
        sprite->rotation = rand() % 360;
        return BlockResult::CONTINUE;
    }

    if (objectName.asString() == "_mouse_") {
        targetX = Input::mousePointer.x;
        targetY = Input::mousePointer.y;
    } else {
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->name == objectName.asString()) {
                targetX = currentSprite->xPosition;
                targetY = currentSprite->yPosition;
                break;
            }
        }
    }

    const double dx = targetX - sprite->xPosition;
    const double dy = targetY - sprite->yPosition;
    sprite->rotation = 90 - (atan2(dy, dx) * 180.0 / M_PI);
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, setrotationstyle) {
    std::string value;
    try {
        value = Scratch::getFieldValue(block, "STYLE");
    } catch (...) {
        Log::logError("Unable to find rotation style.");
        return BlockResult::CONTINUE;
    }

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

    // Not touching any edge
    if (minDist > 0.0) {
        return BlockResult::CONTINUE;
    }

    // Convert current direction to radians
    const double radians = (90.0 - sprite->rotation) * (M_PI / 180.0);
    double dx = std::cos(radians);
    double dy = -std::sin(radians);

    // Reflect the direction based on the edge hit
    if (nearestEdge == "left") {
        dx = std::max<double>(0.2, std::abs(dx));
    } else if (nearestEdge == "right") {
        dx = -std::max<double>(0.2, std::abs(dx));
    } else if (nearestEdge == "top") {
        dy = std::max<double>(0.2, std::abs(dy));
    } else if (nearestEdge == "bottom") {
        dy = -std::max<double>(0.2, std::abs(dy));
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
    Scratch::forceRedraw = true;

    return BlockResult::CONTINUE;
}

SCRATCH_REPORTER_BLOCK(motion, xposition) {
    return Value(sprite->xPosition);
}

SCRATCH_REPORTER_BLOCK(motion, yposition) {
    return Value(sprite->yPosition);
}

SCRATCH_REPORTER_BLOCK(motion, direction) {
    return Value(sprite->rotation);
}
