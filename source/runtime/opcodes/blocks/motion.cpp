#include "../../data/entity_components.hpp"
#include "../../entity_manager.hpp"
#include "../../systems/sprite_system.hpp"
#include "../opcode_registers.hpp"
#include "../opcodes.hpp"
#include "blueprint.hpp"
#include "engine_state.hpp"
#include "math.hpp"

REGISTER_STANDARD_PARSER("motion_movesteps", motion_movesteps, "STEPS")
DEFINE_EXECUTION(motion_movesteps) {
    const Value &stepsValue = thread->stack.back();
    const double steps = stepsValue.asDouble();
    const double angle = Math::degreesToRadians(90.0 - thread->transform->direction);
    SpriteSystem::gotoXY(thread->instanceId, thread->transform->x + std::cos(angle) * steps, thread->transform->y + std::sin(angle) * steps);
    thread->stack.pop_back();
    return BlockResult::YIELD_NEXT;
}

DEFINE_CUSTOM_PARSER("motion_goto", goto_parser) {
    BytecodeChunk chunk;
    CompileResult res = ctx.compileInput("TO");
    if (res.isConstant) {
        std::string to = res.constantValue.asString();
        if (to == "_mouse_") {
            return ParserRegistry::compileStandard(ctx, Opcode::motion_goto_mouse, {}, Purity::Impure);
        } else if (to == "_random_") {
            return ParserRegistry::compileStandard(ctx, Opcode::motion_goto_random, {}, Purity::Impure);
        } else {
            uint16_t targetDefId = 0;
            for (uint16_t i = 0; i < EntityManager::blueprints.size(); ++i) {
                if (EntityManager::blueprints[i].name == to) {
                    targetDefId = i;
                    break;
                }
            }

            chunk.emitOpcode(Opcode::motion_goto_sprite);
            chunk.emit16(static_cast<uint16_t>(targetDefId));

            return CompileResult::Dynamic(std::move(chunk));
        }
    }
    chunk.append(std::move(res.chunk));
    chunk.emitOpcode(Opcode::motion_goto);
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_EXECUTION(motion_goto) {
    Value targetVal = thread->stack.back();
    thread->stack.pop_back();
    std::string target = targetVal.asString();
    if (target == "_mouse_") {
        SpriteSystem::gotoXY(thread->instanceId, Input::mousePointer.x, Input::mousePointer.y);
    } else if (target == "_random_") {
        SpriteSystem::gotoXY(thread->instanceId, rand() % EngineState::projectWidth - EngineState::projectWidth / 2, rand() % EngineState::projectHeight - EngineState::projectHeight / 2);
    } else {
        for (size_t i = 0; i < EntityManager::blueprints.size(); ++i) {
            if (EntityManager::blueprints[i].name == target) {
                const SpriteTransform &transform = EntityManager::transforms[i];
                SpriteSystem::gotoXY(thread->instanceId, transform.x, transform.y);
                break;
            }
        };
    }
    return BlockResult::YIELD_NEXT;
}

DEFINE_EXECUTION(motion_goto_sprite) {
    uint16_t targetDefId = thread->definition->bytecode[thread->pc++];
    SpriteSystem::gotoXY(thread->instanceId, thread->transform->x, thread->transform->y);
    return BlockResult::YIELD_NEXT;
}

DEFINE_EXECUTION(motion_goto_mouse) {
    SpriteSystem::gotoXY(thread->instanceId, Input::mousePointer.x, Input::mousePointer.y);
    return BlockResult::YIELD_NEXT;
}

DEFINE_EXECUTION(motion_goto_random) {
    SpriteSystem::gotoXY(thread->instanceId, rand() % EngineState::projectWidth - EngineState::projectWidth / 2, rand() % EngineState::projectHeight - EngineState::projectHeight / 2);
    return BlockResult::YIELD_NEXT;
}

REGISTER_STANDARD_PARSER("motion_gotoxy", motion_gotoxy, "X", "Y")
DEFINE_EXECUTION(motion_gotoxy) {
    const Value &yValue = thread->stack[thread->stack.size() - 1];
    const Value &xValue = thread->stack[thread->stack.size() - 2];
    SpriteSystem::gotoXY(thread->instanceId, xValue.asDouble(), yValue.asDouble());
    thread->stack.resize(thread->stack.size() - 2);
    return BlockResult::YIELD_NEXT;
}

REGISTER_STANDARD_PARSER("motion_turnleft", motion_turnleft, "DEGREES")
DEFINE_EXECUTION(motion_turnleft) {
    const Value &degreesValue = thread->stack.back();
    SpriteSystem::setDirection(*thread->transform, thread->instanceId, thread->transform->direction - degreesValue.asDouble());
    thread->stack.pop_back();

    return BlockResult::YIELD_NEXT;
}

REGISTER_STANDARD_PARSER("motion_turnright", motion_turnright, "DEGREES")
DEFINE_EXECUTION(motion_turnright) {
    const Value &degreesValue = thread->stack.back();
    SpriteTransform &transform = EntityManager::transforms[thread->instanceId];
    SpriteSystem::setDirection(*thread->transform, thread->instanceId, thread->transform->direction + degreesValue.asDouble());
    thread->stack.pop_back();
    return BlockResult::YIELD_NEXT;
}

REGISTER_STANDARD_PARSER("motion_pointindirection", motion_pointindirection, "DIRECTION")
DEFINE_EXECUTION(motion_pointindirection) {
    const Value &directionValue = thread->stack.back();
    SpriteSystem::setDirection(*thread->transform, thread->instanceId, directionValue.asDouble());
    thread->stack.pop_back();
    return BlockResult::YIELD_NEXT;
}

DEFINE_CUSTOM_PARSER("motion_pointtowards", motion_pointtowards_parser) {
    BytecodeChunk chunk;
    CompileResult res = ctx.compileInput("TOWARDS");
    if (res.isConstant) {
        std::string target = res.constantValue.asString();
        if (target == "_mouse_") {
            return ParserRegistry::compileStandard(ctx, Opcode::motion_pointtowards_mouse, {}, Purity::Impure);
        } else if (target == "_random_") {
            return ParserRegistry::compileStandard(ctx, Opcode::motion_pointtowards_random, {}, Purity::Impure);
        } else {
            uint32_t targetDefId = 0;
            for (size_t i = 0; i < EntityManager::blueprints.size(); ++i) {
                if (EntityManager::blueprints[i].name == target) {
                    targetDefId = i;
                    break;
                }
            }

            chunk.emitPushConstant(Value(static_cast<double>(targetDefId)));
            chunk.emitOpcode(Opcode::motion_pointtowards_sprite);

            return CompileResult::Dynamic(std::move(chunk));
        }
    } else {
        chunk.append(std::move(res.chunk));
        chunk.emitOpcode(Opcode::motion_pointtowards);
        return CompileResult::Dynamic(std::move(chunk));
    }
}

DEFINE_EXECUTION(motion_pointtowards_sprite) {
    Value targetVal = thread->stack.back();
    uint32_t targetDefId = static_cast<uint32_t>(targetVal.asDouble());
    SpriteTransform &transformTarget = EntityManager::transforms[targetDefId];
    const double targetX = transformTarget.x;
    const double targetY = transformTarget.y;
    const double dx = targetX - thread->transform->x;
    const double dy = targetY - thread->transform->y;
    double angle = 90.0 - Math::radiansToDegrees(atan2(dy, dx));
    SpriteSystem::setDirection(*thread->transform, thread->instanceId, angle);
    thread->stack.pop_back();
    return BlockResult::YIELD_NEXT;
}

DEFINE_EXECUTION(motion_pointtowards_random) {
    SpriteTransform &transform = EntityManager::transforms[thread->instanceId];
    SpriteSystem::setDirection(transform, thread->instanceId, rand() % 360);
    return BlockResult::YIELD_NEXT;
}

DEFINE_EXECUTION(motion_pointtowards_mouse) {
    const double dx = Input::mousePointer.x - thread->transform->x;
    const double dy = Input::mousePointer.y - thread->transform->y;
    double angle = 90.0 - Math::radiansToDegrees(atan2(dy, dx));
    SpriteSystem::setDirection(*thread->transform, thread->instanceId, angle);
    return BlockResult::YIELD_NEXT;
}

DEFINE_EXECUTION(motion_pointtowards) {
    Value targetVal = thread->stack.back();
    thread->stack.pop_back();
    std::string target = targetVal.asString();
    if (target == "_mouse_") {
        const double dx = Input::mousePointer.x - thread->transform->x;
        const double dy = Input::mousePointer.y - thread->transform->y;
        double angle = 90.0 - Math::radiansToDegrees(atan2(dy, dx));
        SpriteSystem::setDirection(*thread->transform, thread->instanceId, angle);
    } else if (target == "_random_") {
        SpriteSystem::setDirection(*thread->transform, thread->instanceId, rand() % 360);
    } else {
        for (size_t i = 0; i < EntityManager::blueprints.size(); ++i) {
            if (EntityManager::blueprints[i].name == target) {
                const SpriteTransform &transformTarget = EntityManager::transforms[i];
                const double dx = transformTarget.x - thread->transform->x;
                const double dy = transformTarget.y - thread->transform->y;
                double angle = 90.0 - Math::radiansToDegrees(atan2(dy, dx));
                SpriteSystem::setDirection(*thread->transform, thread->instanceId, angle);
                break;
            }
        };
    }
    return BlockResult::YIELD_NEXT;
}

REGISTER_STANDARD_PARSER("motion_glidesecstoxy", motion_glidesecstoxy, "SECS", "X", "Y")
DEFINE_EXECUTION(motion_glidesecstoxy) {
    if (thread->isGliding) {
        SpriteSystem::gotoXY(thread->instanceId, thread->glideInfo->endX, thread->glideInfo->endY);
        thread->isGliding = false;
        delete thread->glideInfo;
        thread->glideInfo = nullptr;
        return BlockResult::YIELD_NEXT;
    }
    thread->glideInfo = new GlideInfo();

    const Value &yVal = thread->stack[thread->stack.size() - 1];
    const Value &xVal = thread->stack[thread->stack.size() - 2];
    const Value &secsVal = thread->stack[thread->stack.size() - 3];

    float durationSecs = static_cast<float>(secsVal.asDouble());

    if (durationSecs <= 0.0f) {
        SpriteSystem::gotoXY(thread->instanceId, xVal.asDouble(), yVal.asDouble());
        thread->stack.resize(thread->stack.size() - 3);
        return BlockResult::YIELD_NEXT;
    }

    thread->glideInfo->startX = thread->transform->x;
    thread->glideInfo->startY = thread->transform->y;
    thread->glideInfo->endX = static_cast<float>(xVal.asDouble());
    thread->glideInfo->endY = static_cast<float>(yVal.asDouble());
    thread->glideInfo->durationSecs = durationSecs;
    thread->sleepTimer = durationSecs;
    thread->isGliding = true;

    thread->stack.resize(thread->stack.size() - 3);

    thread->state = ThreadState::WAITING_FOR_TIME;
    return BlockResult::YIELD_SAME;
}

DEFINE_CUSTOM_PARSER("motion_glideto", motion_glideto_parser) {
    BytecodeChunk chunk;
    CompileResult res = ctx.compileInput("SECS");
    if (res.isConstant) {
        chunk.emitPushConstant(res.constantValue);
    } else {
        chunk.append(std::move(res.chunk));
    }
    res = ctx.compileInput("TO");
    if (res.isConstant) {
        std::string to = res.constantValue.asString();
        if (to == "_mouse_") {
            chunk.emitOpcode(Opcode::motion_glideto_mouse);
            return CompileResult::Dynamic(std::move(chunk));
        } else if (to == "_random_") {
            chunk.emitOpcode(Opcode::motion_glideto_random);
            return CompileResult::Dynamic(std::move(chunk));
        } else {
            uint32_t targetDefId = 0;
            for (size_t i = 0; i < EntityManager::blueprints.size(); ++i) {
                if (EntityManager::blueprints[i].name == to) {
                    targetDefId = i;
                    break;
                }
            }

            chunk.emitOpcode(Opcode::motion_glideto_sprite);
            chunk.emit16(targetDefId);

            return CompileResult::Dynamic(std::move(chunk));
        }
    }
    chunk.append(std::move(res.chunk));
    chunk.emitOpcode(Opcode::motion_glideto);
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_EXECUTION(motion_glideto_sprite) {
    if (thread->isGliding) {
        SpriteSystem::gotoXY(thread->instanceId, thread->glideInfo->endX, thread->glideInfo->endY);
        thread->isGliding = false;
        delete thread->glideInfo;
        thread->glideInfo = nullptr;
        return BlockResult::YIELD_NEXT;
    }
    TargetDefinition &def = EntityManager::blueprints[thread->defId];
    uint16_t targetDefId = def.bytecode[thread->pc++];

    const Value &secsVal = thread->stack.back();
    const SpriteTransform &targetTransform = EntityManager::transforms[targetDefId];

    float durationSecs = static_cast<float>(secsVal.asDouble());
    const float targetX = targetTransform.x;
    const float targetY = targetTransform.y;

    if (durationSecs <= 0.0f) {
        SpriteSystem::gotoXY(thread->instanceId, targetX, targetY);
        thread->stack.pop_back();
        return BlockResult::YIELD_NEXT;
    }

    thread->glideInfo = new GlideInfo();
    thread->glideInfo->startX = thread->transform->x;
    thread->glideInfo->startY = thread->transform->y;
    thread->glideInfo->endX = targetX;
    thread->glideInfo->endY = targetY;
    thread->glideInfo->durationSecs = durationSecs;
    thread->sleepTimer = durationSecs;
    thread->isGliding = true;

    thread->stack.pop_back();

    thread->state = ThreadState::WAITING_FOR_TIME;
    return BlockResult::YIELD_SAME;
}

DEFINE_EXECUTION(motion_glideto_random) {
    if (thread->isGliding) {
        SpriteSystem::gotoXY(thread->instanceId, thread->glideInfo->endX, thread->glideInfo->endY);
        thread->isGliding = false;
        delete thread->glideInfo;
        thread->glideInfo = nullptr;
        return BlockResult::YIELD_NEXT;
    }
    thread->glideInfo = new GlideInfo();

    const Value &secsVal = thread->stack.back();
    const float xPos = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 480.0f - 240.0f;
    const float yPos = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 360.0f - 180.0f;

    float durationSecs = static_cast<float>(secsVal.asDouble());

    if (durationSecs <= 0.0f) {
        SpriteSystem::gotoXY(thread->instanceId, xPos, yPos);
        thread->stack.pop_back();
        return BlockResult::YIELD_NEXT;
    }
    thread->glideInfo->startX = thread->transform->x;
    thread->glideInfo->startY = thread->transform->y;
    thread->glideInfo->endX = xPos;
    thread->glideInfo->endY = yPos;
    thread->glideInfo->durationSecs = durationSecs;
    thread->sleepTimer = durationSecs;
    thread->isGliding = true;

    thread->stack.pop_back();

    thread->state = ThreadState::WAITING_FOR_TIME;
    return BlockResult::YIELD_SAME;
}

DEFINE_EXECUTION(motion_glideto_mouse) {
    if (thread->isGliding) {
        SpriteSystem::gotoXY(thread->instanceId, thread->glideInfo->endX, thread->glideInfo->endY);
        thread->isGliding = false;
        delete thread->glideInfo;
        thread->glideInfo = nullptr;
        return BlockResult::YIELD_NEXT;
    }
    thread->glideInfo = new GlideInfo();

    const Value &secsVal = thread->stack.back();
    const float xPos = Input::mousePointer.x;
    const float yPos = Input::mousePointer.y;

    float durationSecs = static_cast<float>(secsVal.asDouble());

    if (durationSecs <= 0.0f) {
        SpriteSystem::gotoXY(thread->instanceId, xPos, yPos);
        thread->stack.pop_back();
        return BlockResult::YIELD_NEXT;
    }
    thread->glideInfo->startX = thread->transform->x;
    thread->glideInfo->startY = thread->transform->y;
    thread->glideInfo->endX = xPos;
    thread->glideInfo->endY = yPos;
    thread->glideInfo->durationSecs = durationSecs;
    thread->sleepTimer = durationSecs;
    thread->isGliding = true;

    thread->stack.pop_back();

    thread->state = ThreadState::WAITING_FOR_TIME;
    return BlockResult::YIELD_SAME;
}

DEFINE_EXECUTION(motion_glideto) {
    if (thread->isGliding) {
        SpriteSystem::gotoXY(thread->instanceId, thread->glideInfo->endX, thread->glideInfo->endY);
        thread->isGliding = false;
        delete thread->glideInfo;
        thread->glideInfo = nullptr;
        return BlockResult::YIELD_NEXT;
    }
    thread->glideInfo = new GlideInfo();
    const std::string toVal = thread->stack.back().asString();
    thread->stack.pop_back();
    const Value &secsVal = thread->stack.back();
    float xPos = 0;
    float yPos = 0;
    if (toVal == "_mouse_") {
        xPos = Input::mousePointer.x;
        yPos = Input::mousePointer.y;
    } else if (toVal == "_random_") {
        xPos = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 480.0f - 240.0f;
        yPos = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 360.0f - 180.0f;
    } else {
        uint32_t targetDefId = 0;
        for (size_t i = 0; i < EntityManager::blueprints.size(); ++i) {
            if (EntityManager::blueprints[i].name == toVal) {
                targetDefId = i;
                break;
            }
        }
        const SpriteTransform &targetTransform = EntityManager::transforms[targetDefId];
        xPos = targetTransform.x;
        yPos = targetTransform.y;
    }

    float durationSecs = static_cast<float>(secsVal.asDouble());

    if (durationSecs <= 0.0f) {
        SpriteSystem::gotoXY(thread->instanceId, xPos, yPos);
        thread->stack.pop_back();
        return BlockResult::YIELD_NEXT;
    }

    thread->glideInfo->startX = thread->transform->x;
    thread->glideInfo->startY = thread->transform->y;
    thread->glideInfo->endX = xPos;
    thread->glideInfo->endY = yPos;
    thread->glideInfo->durationSecs = durationSecs;
    thread->sleepTimer = durationSecs;
    thread->isGliding = true;

    thread->stack.pop_back();

    thread->state = ThreadState::WAITING_FOR_TIME;
    return BlockResult::YIELD_SAME;
}

REGISTER_STANDARD_PARSER("motion_ifonedgebounce", motion_ifonedgebounce)
DEFINE_EXECUTION(motion_ifonedgebounce) {
    const double halfWidth = EngineState::projectWidth / 2.0;
    const double halfHeight = EngineState::projectHeight / 2.0;

    const Costume &costume = thread->definition->costumes[thread->renderInfo->costumeId];
    const double scale = (thread->transform->size / 100.0) / costume.bitmapResolution;
    const double spriteHalfWidth = (thread->transform->width * scale) / 2.0;
    const double spriteHalfHeight = (thread->transform->height * scale) / 2.0;

    // Compute bounds of the sprite
    const double left = thread->transform->x - spriteHalfWidth;
    const double right = thread->transform->x + spriteHalfWidth;
    const double top = thread->transform->y + spriteHalfHeight;
    const double bottom = thread->transform->y - spriteHalfHeight;

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

    if (!EntityManager::isColliding(EntityManager::CollisionMode::EDGE, thread->instanceId, 0))
        return BlockResult::CONTINUE;

    // Convert current direction to radians
    const double radians = Math::degreesToRadians(90.0 - thread->transform->direction);
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
    thread->transform->direction = Math::radiansToDegrees(atan2(dy, dx)) + 90.0;

    // Clamp sprite back into stage bounds
    double dxCorrection = 0;
    double dyCorrection = 0;

    if (left < -halfWidth) dxCorrection += -halfWidth - left;
    if (right > halfWidth) dxCorrection += halfWidth - right;
    if (top > halfHeight) dyCorrection += halfHeight - top;
    if (bottom < -halfHeight) dyCorrection += -halfHeight - bottom;

    SpriteSystem::gotoXY(thread->instanceId, thread->transform->x + dxCorrection, thread->transform->y + dyCorrection);
    return BlockResult::CONTINUE;
}

DEFINE_CUSTOM_PARSER("motion_setrotationstyle", setRotationStyle_parser) {
    std::string style = ctx.resolveFieldValue("STYLE");
    if (style == "all-around") {
        return ParserRegistry::compileStandard(ctx, Opcode::motion_setrotationstyle_allaround, {}, Purity::Impure);
    } else if (style == "dont-rotate") {
        return ParserRegistry::compileStandard(ctx, Opcode::motion_setrotationstyle_dontrotate, {}, Purity::Impure);
    }
    return ParserRegistry::compileStandard(ctx, Opcode::motion_setrotationstyle_leftright, {}, Purity::Impure);
}
DEFINE_EXECUTION(motion_setrotationstyle_leftright) {
    EntityManager::transforms[thread->instanceId].rotationStyle = RotationStyle::LEFT_RIGHT;
    EntityManager::renderInfo[thread->instanceId].makeRotationDirty();
    return BlockResult::CONTINUE; // IDK if YIELD_NEXT is needed here, i dont think so. To lazy to check
}

DEFINE_EXECUTION(motion_setrotationstyle_allaround) {
    EntityManager::transforms[thread->instanceId].rotationStyle = RotationStyle::ALL_AROUND;
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(motion_setrotationstyle_dontrotate) {
    EntityManager::transforms[thread->instanceId].rotationStyle = RotationStyle::NONE;
    return BlockResult::CONTINUE;
}

REGISTER_STANDARD_PARSER("motion_xposition", motion_xposition)
DEFINE_EXECUTION(motion_xposition) {
    const auto &transform = EntityManager::transforms[thread->instanceId];
    double rounded = std::round(transform.x);
    double delta = std::fabs(transform.x - rounded);
    thread->stack.emplace_back(Value((delta < 1e-9) ? rounded : transform.x));
    return BlockResult::YIELD_NEXT;
}

REGISTER_STANDARD_PARSER("motion_yposition", motion_yposition)
DEFINE_EXECUTION(motion_yposition) {
    const auto &transform = EntityManager::transforms[thread->instanceId];
    double rounded = std::round(transform.y);
    double delta = std::fabs(transform.y - rounded);
    thread->stack.emplace_back(Value((delta < 1e-9) ? rounded : transform.y));
    return BlockResult::YIELD_NEXT;
}

REGISTER_STANDARD_PARSER("motion_direction", motion_direction, "Y")
DEFINE_EXECUTION(motion_direction) {
    const auto &transform = EntityManager::transforms[thread->instanceId];
    thread->stack.emplace_back(Value(transform.direction));
    return BlockResult::YIELD_NEXT;
}

REGISTER_STANDARD_PARSER("motion_setx", motion_setx, "X")
DEFINE_EXECUTION(motion_setx) {
    double x = thread->stack.back().asDouble();
    thread->stack.pop_back();
    EntityManager::transforms[thread->instanceId].x = x;
    EntityManager::renderInfo[thread->instanceId].makePositionDirty();
    return BlockResult::CONTINUE;
}

REGISTER_STANDARD_PARSER("motion_sety", motion_sety, "Y")
DEFINE_EXECUTION(motion_sety) {
    double y = thread->stack.back().asDouble();
    thread->stack.pop_back();
    EntityManager::transforms[thread->instanceId].y = y;
    EntityManager::renderInfo[thread->instanceId].makePositionDirty();
    return BlockResult::CONTINUE;
}

REGISTER_STANDARD_PARSER("motion_changexby", motion_changexby, "DX")
DEFINE_EXECUTION(motion_changexby) {
    double dx = thread->stack.back().asDouble();
    thread->stack.pop_back();
    EntityManager::transforms[thread->instanceId].x += dx;
    EntityManager::renderInfo[thread->instanceId].makePositionDirty();
    return BlockResult::CONTINUE;
}

REGISTER_STANDARD_PARSER("motion_changeyby", motion_changeyby, "DY")
DEFINE_EXECUTION(motion_changeyby) {
    double dy = thread->stack.back().asDouble();
    thread->stack.pop_back();
    EntityManager::transforms[thread->instanceId].y += dy;
    EntityManager::renderInfo[thread->instanceId].makePositionDirty();
    return BlockResult::CONTINUE;
}

DEFINE_CUSTOM_PARSER("motion_pointtowards_menu", pointTowardsMenu_parser) {
    return CompileResult::Constant(Value(ctx.resolveFieldValue("TOWARDS")));
}
DEFINE_CUSTOM_PARSER("motion_glideto_menu", glideToMenu_parser) {
    return CompileResult::Constant(Value(ctx.resolveFieldValue("TO")));
}
DEFINE_CUSTOM_PARSER("motion_goto_menu", glideTo_parser) {
    return CompileResult::Constant(Value(ctx.resolveFieldValue("TO")));
}