#include "blockUtils.hpp"
#include "runtime/blockExecutor.hpp"
#include <audio.hpp>
#include <blockExecutor.hpp>
#include <input.hpp>
#include <iostream>
#include <math.hpp>
#include <os.hpp>
#include <ostream>
#include <types.hpp>
#include <value.hpp>

SCRATCH_BLOCK(motion, movesteps) {
    double steps;
    if (!Scratch::getInputValueAs(block, "STEPS", thread, sprite, steps)) return BlockResult::REPEAT;
    const double angle = Math::degreesToRadians(90 - sprite->rotation);
    Scratch::gotoXY(sprite, sprite->xPosition + std::cos(angle) * steps, sprite->yPosition + std::sin(angle) * steps);

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(motion, movesteps, {"STEPS", BytecodeStackKind::Number});
SCRATCH_BLOCK_POSITIONAL_N_VOID(motion, movesteps) {
    const double angle = Math::degreesToRadians(90 - sprite->rotation);
    Scratch::gotoXY(sprite, sprite->xPosition + std::cos(angle) * num, sprite->yPosition + std::sin(angle) * num);
}

SCRATCH_BLOCK(motion, goto) {
    std::string object;
    if (!Scratch::getInputValueAs(block, "TO", thread, sprite, object)) return BlockResult::REPEAT;
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
SCRATCH_BLOCK_SIGNATURE(motion, goto, {"TO", BytecodeStackKind::String});
SCRATCH_BLOCK_POSITIONAL_S_VOID(motion, goto) {
    if (str == "_random_") {
        Scratch::gotoXY(sprite, rand() % Scratch::projectWidth - Scratch::projectWidth / 2, rand() % Scratch::projectHeight - Scratch::projectHeight / 2);
    } else if (str == "_mouse_") {
        Scratch::gotoXY(sprite, Input::mousePointer.x, Input::mousePointer.y);
    } else {
        for (Sprite *currentSprite : Scratch::sprites) {
            if (currentSprite->isClone || currentSprite->name != str) continue;
            Scratch::gotoXY(sprite, currentSprite->xPosition, currentSprite->yPosition);
            break;
        }
    }
}

SCRATCH_BLOCK(motion, gotoxy) {
    double x, y;
    if (!Scratch::getInputValueAs(block, "X", thread, sprite, x) ||
        !Scratch::getInputValueAs(block, "Y", thread, sprite, y)) return BlockResult::REPEAT;
    Scratch::gotoXY(sprite, x, y);

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(motion, gotoxy, {"X", BytecodeStackKind::Number}, {"Y", BytecodeStackKind::Number});
SCRATCH_BLOCK_POSITIONAL_NN_VOID(motion, gotoxy) {
    Scratch::gotoXY(sprite, num1, num2);
}

SCRATCH_BLOCK(motion, turnleft) {
    double dir;
    if (!Scratch::getInputValueAs(block, "DEGREES", thread, sprite, dir)) return BlockResult::REPEAT;
    Scratch::setDirection(sprite, sprite->rotation - dir);

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(motion, turnleft, {"DEGREES", BytecodeStackKind::Number});
SCRATCH_BLOCK_POSITIONAL_N_VOID(motion, turnleft) {
    Scratch::setDirection(sprite, sprite->rotation - num);
}

SCRATCH_BLOCK(motion, turnright) {
    double dir;
    if (!Scratch::getInputValueAs(block, "DEGREES", thread, sprite, dir)) return BlockResult::REPEAT;
    Scratch::setDirection(sprite, sprite->rotation + dir);

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(motion, turnright, {"DEGREES", BytecodeStackKind::Number});
SCRATCH_BLOCK_POSITIONAL_N_VOID(motion, turnright) {
    Scratch::setDirection(sprite, sprite->rotation + num);
}

SCRATCH_BLOCK(motion, pointindirection) {
    double dir;
    if (!Scratch::getInputValueAs(block, "DIRECTION", thread, sprite, dir)) return BlockResult::REPEAT;
    Scratch::setDirection(sprite, dir);

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(motion, pointindirection, {"DIRECTION", BytecodeStackKind::Number});
SCRATCH_BLOCK_POSITIONAL_N_VOID(motion, pointindirection) {
    Scratch::setDirection(sprite, num);
}

SCRATCH_BLOCK(motion, changexby) {
    double dx;
    if (!Scratch::getInputValueAs(block, "DX", thread, sprite, dx)) return BlockResult::REPEAT;
    Scratch::gotoXY(sprite, sprite->xPosition + dx, sprite->yPosition);

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(motion, changexby, {"DX", BytecodeStackKind::Number});
SCRATCH_BLOCK_POSITIONAL_N_VOID(motion, changexby) {
    Scratch::gotoXY(sprite, sprite->xPosition + num, sprite->yPosition);
}

SCRATCH_BLOCK(motion, changeyby) {
    double dy;
    if (!Scratch::getInputValueAs(block, "DY", thread, sprite, dy)) return BlockResult::REPEAT;
    Scratch::gotoXY(sprite, sprite->xPosition, sprite->yPosition + dy);

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(motion, changeyby, {"DY", BytecodeStackKind::Number});
SCRATCH_BLOCK_POSITIONAL_N_VOID(motion, changeyby) {
    Scratch::gotoXY(sprite, sprite->xPosition, sprite->yPosition + num);
}

SCRATCH_BLOCK(motion, setx) {
    double x;
    if (!Scratch::getInputValueAs(block, "X", thread, sprite, x)) return BlockResult::REPEAT;
    Scratch::gotoXY(sprite, x, sprite->yPosition);

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(motion, setx, {"X", BytecodeStackKind::Number});
SCRATCH_BLOCK_POSITIONAL_N_VOID(motion, setx) {
    Scratch::gotoXY(sprite, num, sprite->yPosition);
}

SCRATCH_BLOCK(motion, sety) {
    double y;
    if (!Scratch::getInputValueAs(block, "Y", thread, sprite, y)) return BlockResult::REPEAT;
    Scratch::gotoXY(sprite, sprite->xPosition, y);

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(motion, sety, {"Y", BytecodeStackKind::Number});
SCRATCH_BLOCK_POSITIONAL_N_VOID(motion, sety) {
    Scratch::gotoXY(sprite, sprite->xPosition, num);
}

SCRATCH_BLOCK(motion, glideto) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps == 1) {
    glide:
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

    double duration;
    std::string input;
    if (!Scratch::getInputValueAs(block, "SECS", thread, sprite, duration) ||
        !Scratch::getInputValueAs(block, "TO", thread, sprite, input)) return BlockResult::REPEAT;

    state->waitDuration = duration * 1000;

    state->waitTimer.start();
    state->glideStartX = sprite->xPosition;
    state->glideStartY = sprite->yPosition;

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
    Scratch::resetInput(block);
    goto glide;
    return BlockResult::REPEAT;
}
SCRATCH_BLOCK_BLOCKING_SIGNATURE(motion, glideto, {"SECS", BytecodeStackKind::Number}, {"TO", BytecodeStackKind::String});

SCRATCH_BLOCK(motion, glidesecstoxy) {
    BlockState *state = thread->getState(block);

    if (state->completedSteps == 1) {
    glide:
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
    double duration, x, y;
    if (!Scratch::getInputValueAs(block, "SECS", thread, sprite, duration) ||
        !Scratch::getInputValueAs(block, "X", thread, sprite, x) ||
        !Scratch::getInputValueAs(block, "Y", thread, sprite, y)) return BlockResult::REPEAT;
    state->waitDuration = duration * 1000;
    state->glideEndX = x;
    state->glideEndY = y;
    state->waitTimer.start();
    state->glideStartX = sprite->xPosition;
    state->glideStartY = sprite->yPosition;

    state->completedSteps = 1;
    Scratch::resetInput(block);

    goto glide;
    return BlockResult::REPEAT;
}
SCRATCH_BLOCK_BLOCKING_SIGNATURE(motion, glidesecstoxy, {"SECS", BytecodeStackKind::Number}, {"X", BytecodeStackKind::Number}, {"Y", BytecodeStackKind::Number});

SCRATCH_BLOCK(motion, pointtowards) {
    std::string objectName;
    if (!Scratch::getInputValueAs(block, "TOWARDS", thread, sprite, objectName)) return BlockResult::REPEAT;

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
SCRATCH_BLOCK_SIGNATURE(motion, pointtowards, {"TOWARDS", BytecodeStackKind::String});
SCRATCH_BLOCK_POSITIONAL_S_VOID(motion, pointtowards) {
    double targetX = 0;
    double targetY = 0;

    if (str == "_random_") {
        sprite->rotation = (rand() % 360) - 179.0f;
        return;
    }

    if (str == "_mouse_") {
        targetX = Input::mousePointer.x;
        targetY = Input::mousePointer.y;
    } else {
        for (Sprite *currentSprite : Scratch::sprites) {
            if (!currentSprite->isClone && currentSprite->name == str) {
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
SCRATCH_BYTECODE_BLOCK(motion, setrotationstyle) {
    const std::string rotationType = Scratch::getFieldValue(*block, "STYLE");

    BytecodeIndex style;
    if (rotationType == "left-right") style = 0;
    else if (rotationType == "don't rotate") style = 1;
    else style = 2;

    outFirstInstr = compiler.emit(BytecodeOp::SetRotationStyle, invalidBytecodeIndex, style);
    return true;
}

SCRATCH_BLOCK(motion, ifonedgebounce) {
    const double halfWidth = Scratch::projectWidth / 2.0;
    const double halfHeight = Scratch::projectHeight / 2.0;

    const Costume &costume = sprite->costumes[sprite->currentCostume];
#ifdef __XBOX__
    double scale = (sprite->size / 100.0) / costume.bitmapResolution;
    if (Scratch::bitmapHalfQuality && !costume.isSVG && costume.bitmapResolution == 2) {
        scale *= 2.0;
    }
#else
    const double scale = (sprite->size / 100.0) / costume.bitmapResolution;
#endif
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
SCRATCH_BLOCK_SIGNATURE(motion, ifonedgebounce);
SCRATCH_BLOCK_POSITIONAL_VOID(motion, ifonedgebounce) {
    const double halfWidth = Scratch::projectWidth / 2.0;
    const double halfHeight = Scratch::projectHeight / 2.0;

    const Costume &costume = sprite->costumes[sprite->currentCostume];
    const double scale = (sprite->size / 100.0) / costume.bitmapResolution;
    const double spriteHalfWidth = (sprite->spriteWidth * scale) / 2.0;
    const double spriteHalfHeight = (sprite->spriteHeight * scale) / 2.0;

    const double left = sprite->xPosition - spriteHalfWidth;
    const double right = sprite->xPosition + spriteHalfWidth;
    const double top = sprite->yPosition + spriteHalfHeight;
    const double bottom = sprite->yPosition - spriteHalfHeight;

    const double distLeft = std::max(0.0, halfWidth + left);
    const double distRight = std::max(0.0, halfWidth - right);
    const double distTop = std::max(0.0, halfHeight - top);
    const double distBottom = std::max(0.0, halfHeight + bottom);

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
        return;

    const double radians = Math::degreesToRadians(90.0 - sprite->rotation);
    double dx = std::cos(radians);
    double dy = -std::sin(radians);

    if (nearestEdge == "left") {
        dx = std::max(0.2, std::fabs(dx));
    } else if (nearestEdge == "right") {
        dx = -std::max(0.2, std::fabs(dx));
    } else if (nearestEdge == "top") {
        dy = std::max(0.2, std::fabs(dy));
    } else if (nearestEdge == "bottom") {
        dy = -std::max(0.2, std::fabs(dy));
    }

    sprite->rotation = Math::radiansToDegrees(atan2(dy, dx)) + 90.0;

    double dxCorrection = 0;
    double dyCorrection = 0;

    if (left < -halfWidth) dxCorrection += -halfWidth - left;
    if (right > halfWidth) dxCorrection += halfWidth - right;
    if (top > halfHeight) dyCorrection += halfHeight - top;
    if (bottom < -halfHeight) dyCorrection += -halfHeight - bottom;

    Scratch::gotoXY(sprite, sprite->xPosition + dxCorrection, sprite->yPosition + dyCorrection);
}

SCRATCH_BLOCK_POSITIONAL_SPRITE_N(motion, xposition) {
    const double rounded = ::round(sprite->xPosition);
    const double delta = std::fabs(sprite->xPosition - rounded);
    return (delta < 1e-9) ? rounded : sprite->xPosition;
}
SCRATCH_BLOCK_DOUBLE(motion, xposition) {
    *outValue = block_motion_xposition_pos_(sprite);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(motion, xposition);

SCRATCH_BLOCK_POSITIONAL_SPRITE_N(motion, yposition) {
    const double rounded = ::round(sprite->yPosition);
    const double delta = std::fabs(sprite->yPosition - rounded);
    return (delta < 1e-9) ? rounded : sprite->yPosition;
}
SCRATCH_BLOCK_DOUBLE(motion, yposition) {
    *outValue = block_motion_yposition_pos_(sprite);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(motion, yposition);

SCRATCH_BLOCK_POSITIONAL_SPRITE_N(motion, direction) {
    return sprite->rotation;
}
SCRATCH_BLOCK_DOUBLE(motion, direction) {
    *outValue = block_motion_direction_pos_(sprite);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(motion, direction);

SCRATCH_BLOCK(motion, xscroll) {
    *outValue = Value(Undefined{});
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(motion, xscroll);

SCRATCH_BLOCK(motion, yscroll) {
    *outValue = Value(Undefined{});
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(motion, yscroll);

SCRATCH_SHADOW_BLOCK(motion_goto_menu, TO)
SCRATCH_SHADOW_BLOCK(motion_glideto_menu, TO)
SCRATCH_SHADOW_BLOCK(motion_pointtowards_menu, TOWARDS)
