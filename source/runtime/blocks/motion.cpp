#include "blockUtils.hpp"
#include "runtime/blockExecutor.hpp"
#include <audio.hpp>
#include <blockExecutor.hpp>
#include <collision.hpp>
#include <input.hpp>
#include <iostream>
#include <math.hpp>
#include <os.hpp>
#include <ostream>
#include <types.hpp>
#include <value.hpp>

static bool targetPosition(const std::string &name, double &x, double &y) {
    if (name == "_mouse_") {
        x = Input::mousePointer.x;
        y = Input::mousePointer.y;
        return true;
    }
    if (name == "_random_") {
        x = std::round(Scratch::projectWidth * (rand() / (RAND_MAX + 1.0) - 0.5));
        y = std::round(Scratch::projectHeight * (rand() / (RAND_MAX + 1.0) - 0.5));
        return true;
    }
    for (Sprite *currentSprite : Scratch::sprites) {
        if (currentSprite->isClone || currentSprite->isStage || currentSprite->name != name) continue;
        x = currentSprite->xPosition;
        y = currentSprite->yPosition;
        return true;
    }
    return false;
}

static void pointTowardsName(Sprite *sprite, const std::string &name) {
    if (name == "_random_") {
        Scratch::setDirection(sprite, std::round(rand() / (RAND_MAX + 1.0) * 360) - 180);
        return;
    }

    double targetX, targetY;
    if (!targetPosition(name, targetX, targetY)) return;

    const double dx = targetX - sprite->xPosition;
    const double dy = targetY - sprite->yPosition;
    Scratch::setDirection(sprite, 90 - Math::radiansToDegrees(atan2(dy, dx)));
}

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
    double x, y;
    if (targetPosition(object, x, y)) Scratch::gotoXY(sprite, x, y);

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(motion, goto, {"TO", BytecodeStackKind::String});
SCRATCH_BLOCK_POSITIONAL_S_VOID(motion, goto) {
    double x, y;
    if (targetPosition(str, x, y)) Scratch::gotoXY(sprite, x, y);
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

    double positionXStr, positionYStr;
    if (!targetPosition(input, positionXStr, positionYStr)) {
        thread->eraseState(block);
        Scratch::resetInput(block);
        return BlockResult::CONTINUE;
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
    pointTowardsName(sprite, objectName);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(motion, pointtowards, {"TOWARDS", BytecodeStackKind::String});
SCRATCH_BLOCK_POSITIONAL_S_VOID(motion, pointtowards) {
    pointTowardsName(sprite, str);
}

SCRATCH_BLOCK(motion, setrotationstyle) {
    const std::string rotationType = Scratch::getFieldValue(*block, "STYLE");

    if (rotationType == "left-right")
        sprite->rotationStyle = sprite->LEFT_RIGHT;
    else if (rotationType == "don't rotate")
        sprite->rotationStyle = sprite->NONE;
    else if (rotationType == "all around")
        sprite->rotationStyle = sprite->ALL_AROUND;

    return BlockResult::CONTINUE;
}
SCRATCH_BYTECODE_BLOCK(motion, setrotationstyle) {
    const std::string rotationType = Scratch::getFieldValue(*block, "STYLE");

    BytecodeIndex style;
    if (rotationType == "left-right") style = 0;
    else if (rotationType == "don't rotate") style = 1;
    else if (rotationType == "all around") style = 2;
    else {
        outFirstInstr = compiler.here();
        return true;
    }

    outFirstInstr = compiler.emit(BytecodeOp::SetRotationStyle, invalidBytecodeIndex, style);
    return true;
}

static void ifOnEdgeBounce(Sprite *sprite) {
    if (sprite->spriteWidth == 0 || sprite->spriteHeight == 0) Scratch::loadCurrentCostumeImage(sprite);

    const double halfWidth = Scratch::projectWidth / 2.0;
    const double halfHeight = Scratch::projectHeight / 2.0;

    const collision::AABB bounds = collision::getFenceBounds(sprite);

    const double distLeft = std::max(0.0, halfWidth + bounds.left);
    const double distTop = std::max(0.0, halfHeight - bounds.top);
    const double distRight = std::max(0.0, halfWidth - bounds.right);
    const double distBottom = std::max(0.0, halfHeight + bounds.bottom);

    std::string nearestEdge;
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

    if (minDist > 0) return;

    const double radians = Math::degreesToRadians(90.0 - sprite->rotation);
    double dx = std::cos(radians);
    double dy = -std::sin(radians);

    if (nearestEdge == "left") {
        dx = std::max(0.2, std::fabs(dx));
    } else if (nearestEdge == "top") {
        dy = std::max(0.2, std::fabs(dy));
    } else if (nearestEdge == "right") {
        dx = 0 - std::max(0.2, std::fabs(dx));
    } else if (nearestEdge == "bottom") {
        dy = 0 - std::max(0.2, std::fabs(dy));
    }

    Scratch::setDirection(sprite, Math::radiansToDegrees(atan2(dy, dx)) + 90.0);

    double dxCorrection = 0;
    double dyCorrection = 0;
    if (bounds.left < -halfWidth) dxCorrection += -halfWidth - bounds.left;
    if (bounds.right > halfWidth) dxCorrection += halfWidth - bounds.right;
    if (bounds.top > halfHeight) dyCorrection += halfHeight - bounds.top;
    if (bounds.bottom < -halfHeight) dyCorrection += -halfHeight - bounds.bottom;

    Scratch::gotoXY(sprite, sprite->xPosition + dxCorrection, sprite->yPosition + dyCorrection);
}

SCRATCH_BLOCK(motion, ifonedgebounce) {
    ifOnEdgeBounce(sprite);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(motion, ifonedgebounce);
SCRATCH_BLOCK_POSITIONAL_VOID(motion, ifonedgebounce) {
    ifOnEdgeBounce(sprite);
}

SCRATCH_BLOCK_POSITIONAL_SPRITE_N(motion, xposition) {
    const double rounded = std::round(sprite->xPosition);
    const double delta = std::fabs(sprite->xPosition - rounded);
    return (delta < 1e-9) ? rounded : sprite->xPosition;
}
SCRATCH_BLOCK_DOUBLE(motion, xposition) {
    *outValue = block_motion_xposition_pos_(sprite);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(motion, xposition);

SCRATCH_BLOCK_POSITIONAL_SPRITE_N(motion, yposition) {
    const double rounded = std::round(sprite->yPosition);
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
