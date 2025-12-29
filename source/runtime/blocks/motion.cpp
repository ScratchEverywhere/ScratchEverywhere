#include "blockUtils.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <input.hpp>
#include <runtime.hpp>
#include <math.h>
#include <math.hpp>
#include <render.hpp>
#include <sprite.hpp>
#include <string>
#include <value.hpp>

SCRATCH_BLOCK(motion, movesteps)  {
    BlockResult res = block.getInput("STEPS", thread, sprite);
    if (res.progress != Progress::CONTINUE) return res;
    Value steps = res.value;

    const double oldX = sprite->xPosition;
    const double oldY = sprite->yPosition;

    if (!steps.isNumeric()) return BlockResult(Value(), Progress::CONTINUE);
    double angle = (sprite->rotation - 90) * M_PI / 180.0;
    sprite->xPosition += std::cos(angle) * steps.asDouble();
    sprite->yPosition -= std::sin(angle) * steps.asDouble();
    if (Scratch::fencing) Scratch::fenceSpriteWithinBounds(sprite);

    if (sprite->penData.down && (oldX != sprite->xPosition || oldY != sprite->yPosition)) Render::penMove(oldX, oldY, sprite->xPosition, sprite->yPosition, sprite);
    Scratch::forceRedraw = true;

    return BlockResult(Value(), Progress::CONTINUE);
}