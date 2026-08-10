#include "sprite_system.hpp"

#include "../core/collision.hpp"
#include "../entity_manager.hpp"
#include "../vm/engine_state.hpp"

#include "costume_system.hpp"
#include "render.hpp"

namespace SpriteSystem {

void gotoXY(uint32_t instanceId, double x, double y) {
    SpriteTransform &transform = EntityManager::transforms[instanceId];
    if (transform.isStage()) return;

    const double oldX = transform.x;
    const double oldY = transform.y;
    transform.x = x;
    transform.y = y;

    if (EngineState::fencing) fenceSpriteWithinBounds(transform, instanceId);

    if (EntityManager::pen[instanceId].down && (oldX != transform.x || oldY != transform.y)) {
        if (EngineState::accuratePen) Render::penMoveAccurate(oldX, oldY, transform.x, transform.y, instanceId);
        else Render::penMoveFast(oldX, oldY, transform.x, transform.y, instanceId);
    }
    RenderInfo &renderInfo = EntityManager::renderInfo[instanceId];
    if (renderInfo.isVisible()) EngineState::forceRedraw = true;
    renderInfo.makePositionDirty();
}

void fenceSpriteWithinBounds(SpriteTransform &transform, uint32_t instanceId) {
    if (std::abs(transform.x) < EngineState::projectWidth * 0.3 && std::abs(transform.y) < EngineState::projectHeight * 0.3)
        return;

    if (transform.width == 0 || transform.height == 0) CostumeSystem::loadCurrentCostumeImage(instanceId);

    collision::AABB spriteBounds = collision::getSpriteBounds(instanceId);
    constexpr float fenceWidth = 15.0f;

    const float width = spriteBounds.right - spriteBounds.left;
    const float height = spriteBounds.top - spriteBounds.bottom;
    const float inset = std::floor(std::min(width, height) * 0.5f);

    const float sx = (EngineState::projectWidth * 0.5f) - std::min(fenceWidth, inset);
    const float sy = (EngineState::projectHeight * 0.5f) - std::min(fenceWidth, inset);

    float newX = transform.x;
    float newY = transform.y;

    if (spriteBounds.right < -sx) {
        newX = std::ceil(transform.x - (sx + spriteBounds.right));
    } else if (spriteBounds.left > sx) {
        newX = std::floor(transform.x + (sx - spriteBounds.left));
    }

    if (spriteBounds.top < -sy) {
        newY = std::ceil(transform.y - (sy + spriteBounds.top));
    } else if (spriteBounds.bottom > sy) {
        newY = std::floor(transform.y + (sy - spriteBounds.bottom));
    }

    transform.x = newX;
    transform.y = newY;
}

void setDirection(SpriteTransform &transform, uint32_t instanceId, double direction) {
    if (transform.isStage()) return;
    if (direction == std::numeric_limits<double>::infinity() || direction == -std::numeric_limits<double>::infinity() || std::isnan(direction)) {
        return;
    }
    RenderInfo &renderInfo = EntityManager::renderInfo[instanceId];

    transform.direction = direction - floor((direction + 179) / 360) * 360;
    if (renderInfo.isVisible()) EngineState::forceRedraw = true;
    renderInfo.makeRotationDirty();
}

void switchCostume(uint32_t instanceId, uint32_t costumeIndex) {
    RenderInfo &renderInfo = EntityManager::renderInfo[instanceId];
    renderInfo.costumeId = std::round(costumeIndex);

    TargetDefinition &blueprint = EntityManager::blueprints[EntityManager::blueprintIds[instanceId]];
    renderInfo.costumeId = std::isfinite(renderInfo.costumeId) ? (renderInfo.costumeId - std::floor(renderInfo.costumeId / blueprint.costumes.size()) * blueprint.costumes.size()) : 0;

    CostumeSystem::loadCurrentCostumeImage(instanceId);

    if (renderInfo.isVisible()) EngineState::forceRedraw = true;
    renderInfo.makePositionDirty();
}
} // namespace SpriteSystem