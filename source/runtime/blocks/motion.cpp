#include "blockUtils.hpp"
#include "runtime/blockExecutor.hpp"
#include <audio.hpp>
#include <blockExecutor.hpp>
#include <input.hpp>
#include <iostream>
#include <math.hpp>
#include <os.hpp>
#include <ostream>
#include <sprite.hpp>
#include <value.hpp>

SCRATCH_BLOCK(motion, movesteps) {
    Value stepsValue;
    if (!Scratch::getInput(block, "STEPS", thread, sprite, stepsValue)) return BlockResult::REPEAT;
    const double steps = stepsValue.asDouble();
    const double angle = Math::degreesToRadians(90 - sprite->rotation);
    Scratch::gotoXY(sprite, sprite->xPosition + std::cos(angle) * steps, sprite->yPosition + std::sin(angle) * steps);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, goto) {

    Value objectValue;
    if (!Scratch::getInput(block, "TO", thread, sprite, objectValue)) return BlockResult::REPEAT;
    std::string object = objectValue.asString();
    if (object == "_random_") {
        Scratch::gotoXY(sprite, rand() % Scratch::projectWidth - Scratch::projectWidth / 2, rand() % Scratch::projectHeight - Scratch::projectHeight / 2);
    } else if (object == "_mouse_") {
        Scratch::gotoXY(sprite, Input::mousePointer.x, Input::mousePointer.y);
    } else {
        for (Sprite *currentSprite : Scratch::sprites) {
            if (currentSprite->isClone || currentSprite->name != object) continue;
            Scratch::gotoXY(sprite, currentSprite->xPosition, currentSprite->yPosition);
            break;
        }
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, gotoxy) {
    Value xValue, yValue;
    if (!Scratch::getInput(block, "X", thread, sprite, xValue) ||
        !Scratch::getInput(block, "Y", thread, sprite, yValue)) return BlockResult::REPEAT;
    Scratch::gotoXY(sprite, xValue.asDouble(), yValue.asDouble());

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, turnleft) {
    Value dirValue;
    if (!Scratch::getInput(block, "DEGREES", thread, sprite, dirValue)) return BlockResult::REPEAT;
    Scratch::setDirection(sprite, sprite->rotation - dirValue.asDouble());

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, turnright) {
    Value dirValue;
    if (!Scratch::getInput(block, "DEGREES", thread, sprite, dirValue)) return BlockResult::REPEAT;
    Scratch::setDirection(sprite, sprite->rotation + dirValue.asDouble());

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, pointindirection) {
    Value dirValue;
    if (!Scratch::getInput(block, "DIRECTION", thread, sprite, dirValue)) return BlockResult::REPEAT;
    Scratch::setDirection(sprite, dirValue.asDouble());

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, changexby) {
    Value dxValue;
    if (!Scratch::getInput(block, "DX", thread, sprite, dxValue)) return BlockResult::REPEAT;
    Scratch::gotoXY(sprite, sprite->xPosition + dxValue.asDouble(), sprite->yPosition);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, changeyby) {
    Value dyValue;
    if (!Scratch::getInput(block, "DY", thread, sprite, dyValue)) return BlockResult::REPEAT;
    Scratch::gotoXY(sprite, sprite->xPosition, sprite->yPosition + dyValue.asDouble());

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, setx) {
    Value xValue;
    if (!Scratch::getInput(block, "X", thread, sprite, xValue)) return BlockResult::REPEAT;
    const double X = xValue.asDouble();
    Scratch::gotoXY(sprite, X, sprite->yPosition);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, sety) {
    Value yValue;
    if (!Scratch::getInput(block, "Y", thread, sprite, yValue)) return BlockResult::REPEAT;
    const double Y = yValue.asDouble();
    Scratch::gotoXY(sprite, sprite->xPosition, Y);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, glideto) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps == 1) {
        const int elapsedTime = state->waitTimer.getTimeMs();

        if (elapsedTime >= state->waitDuration) {
            Scratch::gotoXY(sprite, state->glideEndX, state->glideEndY);
            thread->eraseState(block);
            return BlockResult::CONTINUE;
        }

        const double progress = static_cast<double>(elapsedTime) / state->waitDuration;
        Scratch::gotoXY(sprite, state->glideStartX + (state->glideEndX - state->glideStartX) * progress, state->glideStartY + (state->glideEndY - state->glideStartY) * progress);
        return BlockResult::REPEAT;
    }

    Value duration, to;
    if (!Scratch::getInput(block, "SECS", thread, sprite, duration) ||
        !Scratch::getInput(block, "TO", thread, sprite, to)) return BlockResult::REPEAT;

    state->waitDuration = duration.asDouble() * 1000;

    state->waitTimer.start();
    state->glideStartX = sprite->xPosition;
    state->glideStartY = sprite->yPosition;
    const std::string input = to.asString();

    double positionXStr = sprite->xPosition;
    double positionYStr = sprite->yPosition;

    if (input == "_random_") {
        positionXStr = rand() % Scratch::projectWidth - Scratch::projectWidth / 2;
        positionYStr = rand() % Scratch::projectHeight - Scratch::projectHeight / 2;
    } else if (input == "_mouse_") {
        positionXStr = Input::mousePointer.x;
        positionYStr = Input::mousePointer.y;
    } else {
        for (auto &currentSprite : Scratch::sprites) {
            if (currentSprite->isClone || currentSprite->name != input) continue;
            positionXStr = currentSprite->xPosition;
            positionYStr = currentSprite->yPosition;
            break;
        }
    }

    state->glideEndX = positionXStr;
    state->glideEndY = positionYStr;
    state->completedSteps = 1;
    return BlockResult::REPEAT;
}

SCRATCH_BLOCK(motion, glidesecstoxy) {
    BlockState *state = thread->getState(block);

    if (state->completedSteps == 1) {
        const int elapsedTime = state->waitTimer.getTimeMs();
        if (elapsedTime >= state->waitDuration) {
            Scratch::gotoXY(sprite, state->glideEndX, state->glideEndY);
            thread->eraseState(block);
            return BlockResult::CONTINUE;
        }
        double progress = static_cast<double>(elapsedTime) / state->waitDuration;
        Scratch::gotoXY(sprite, state->glideStartX + (state->glideEndX - state->glideStartX) * progress, state->glideStartY + (state->glideEndY - state->glideStartY) * progress);
        return BlockResult::REPEAT;
    }
    Value duration, X, Y;
    if (!Scratch::getInput(block, "SECS", thread, sprite, duration) ||
        !Scratch::getInput(block, "X", thread, sprite, X) ||
        !Scratch::getInput(block, "Y", thread, sprite, Y)) return BlockResult::REPEAT;
    state->waitDuration = duration.asDouble() * 1000;
    state->glideEndX = X.asDouble();
    state->glideEndY = Y.asDouble();
    state->waitTimer.start();
    state->glideStartX = sprite->xPosition;
    state->glideStartY = sprite->yPosition;

    if (state->waitDuration <= 0) return BlockResult::CONTINUE;
    state->completedSteps = 1;

    return BlockResult::REPEAT;
}

SCRATCH_BLOCK(motion, pointtowards) {
    Value towardsValue;
    if (!Scratch::getInput(block, "TOWARDS", thread, sprite, towardsValue)) return BlockResult::REPEAT;
    const std::string objectName = towardsValue.asString();

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
            if (!currentSprite->isClone && currentSprite->name == objectName) {
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
    const std::string rotationType = Scratch::getFieldValue(*block, "STYLE");

    if (rotationType == "left-right")
        sprite->rotationStyle = sprite->LEFT_RIGHT;
    else if (rotationType == "don't rotate")
        sprite->rotationStyle = sprite->NONE;
    else
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

SCRATCH_BLOCK(motion, xposition) {
    double rounded = std::round(sprite->xPosition);
    double delta = std::fabs(sprite->xPosition - rounded);
    *outValue = Value((delta < 1e-9) ? rounded : sprite->xPosition);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, yposition) {
    double rounded = std::round(sprite->yPosition);
    double delta = std::fabs(sprite->yPosition - rounded);
    *outValue = Value((delta < 1e-9) ? rounded : sprite->yPosition);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, direction) {
    *outValue = Value(sprite->rotation);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, xscroll) {
    *outValue = Value(Undefined{});
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(motion, yscroll) {
    *outValue = Value(Undefined{});
    return BlockResult::CONTINUE;
}

SCRATCH_SHADOW_BLOCK(motion_goto_menu, TO)
SCRATCH_SHADOW_BLOCK(motion_glideto_menu, TO)
SCRATCH_SHADOW_BLOCK(motion_pointtowards_menu, TOWARDS)
