#include "entity_manager.hpp"
#include "core/collision.hpp"
#include "opcodes/opcodes.hpp"
#include "vm/engine_state.hpp"
#include "vm/vm.hpp"
#include <render.hpp>
#include <speech_manager.hpp>

uint32_t EntityManager::allocateInstance(uint32_t defId) {
    uint32_t id;
    if (!freeInstances.empty()) {
        id = freeInstances.back();
        freeInstances.pop_back();

        blueprintIds[id] = defId;
        activeInstances[id] = true;
        transforms[id] = {};
        renderInfo[id] = {};
        variables[id].clear();
        lists[id].clear();
        ttsData[id] = {};
        effects[id] = {};
        audio[id] = {};
        pen[id] = {};
    } else {
        id = blueprintIds.size();
        blueprintIds.push_back(defId);
        transforms.push_back({});
        renderInfo.push_back({});
        activeInstances.push_back(true);
        variables.push_back({});
        lists.push_back({});
        ttsData.push_back({});
        effects.push_back({});
        audio.push_back({});
        pen.push_back({});
    }
    return id;
}

uint32_t EntityManager::createInstance(uint32_t defId) {
    uint32_t id = allocateInstance(defId);
    renderOrder.push_back(id);
    syncLayerIndices();
    return id;
}

void EntityManager::queueClone(uint32_t sourceId) {
    if (cloneCount >= EngineState::maxClones) return;
    pendingClones.push_back(sourceId);
}

void EntityManager::queueDeletion(uint32_t instanceId) {
    pendingDeletions.push_back(instanceId);
}

void EntityManager::flushPendingClones() {
    if (pendingClones.empty()) return;

    for (uint32_t sourceId : pendingClones) {
        if (cloneCount >= EngineState::maxClones) break;
        if (sourceId >= activeInstances.size() || !activeInstances[sourceId]) continue;

        cloneCount++;
        uint32_t cloneId = allocateInstance(blueprintIds[sourceId]);

        transforms[cloneId] = transforms[sourceId];
        transforms[cloneId].instanceId = cloneId;
        transforms[cloneId].flags |= FLAG_IS_CLONE;

        renderInfo[cloneId] = renderInfo[sourceId];
        variables[cloneId] = variables[sourceId];
        lists[cloneId] = lists[sourceId];
        effects[cloneId] = effects[sourceId];
        audio[cloneId] = audio[sourceId];
        pen[cloneId] = pen[sourceId];
        ttsData[cloneId] = ttsData[sourceId];

        if (transforms[cloneId].shouldClick()) {
            spritesToClick.push_back(cloneId);
        }

        auto it = std::find(renderOrder.begin(), renderOrder.end(), sourceId);
        if (it != renderOrder.end()) {
            renderOrder.insert(it, cloneId);
        } else {
            renderOrder.push_back(cloneId);
        }

        VM::dispatchEventInSprite(static_cast<uint16_t>(HatType::CLONE_START), 0, cloneId, true, 0);
    }

    pendingClones.clear();
    syncLayerIndices();
}

void EntityManager::syncLayerIndices() {
    for (size_t i = 0; i < renderOrder.size(); ++i) {
        transforms[renderOrder[i]].layer = static_cast<uint32_t>(i);
    }
}

void EntityManager::moveForward(uint32_t instanceId, uint32_t amount) {
    auto it = std::find(renderOrder.begin(), renderOrder.end(), instanceId);
    if (it == renderOrder.end() || amount == 0) return;

    size_t oldIdx = std::distance(renderOrder.begin(), it);
    size_t newIdx = std::min(oldIdx + amount, renderOrder.size() - 1);

    if (oldIdx != newIdx) {
        std::rotate(renderOrder.begin() + oldIdx,
                    renderOrder.begin() + oldIdx + 1,
                    renderOrder.begin() + newIdx + 1);
        syncLayerIndices();
    }
}

void EntityManager::moveBackwards(uint32_t instanceId, uint32_t amount) {
    auto it = std::find(renderOrder.begin(), renderOrder.end(), instanceId);
    if (it == renderOrder.end() || amount == 0) return;

    size_t oldIdx = std::distance(renderOrder.begin(), it);
    size_t newIdx = (amount > oldIdx) ? 0 : (oldIdx - amount);

    if (oldIdx != newIdx) {
        std::rotate(renderOrder.begin() + newIdx,
                    renderOrder.begin() + oldIdx,
                    renderOrder.begin() + oldIdx + 1);
        syncLayerIndices();
    }
}

void EntityManager::goToFrontLayer(uint32_t instanceId) {
    auto it = std::find(renderOrder.begin(), renderOrder.end(), instanceId);
    if (it != renderOrder.end() && it + 1 != renderOrder.end()) {
        std::rotate(it, it + 1, renderOrder.end());
        syncLayerIndices();
    }
}

void EntityManager::goToBackLayer(uint32_t instanceId) {
    auto it = std::find(renderOrder.begin(), renderOrder.end(), instanceId);
    if (it != renderOrder.end() && it != renderOrder.begin()) {
        std::rotate(renderOrder.begin(), it, it + 1);
        syncLayerIndices();
    }
}

void EntityManager::queueBroadcast(uint32_t broadcastId) {
    pendingBroadcasts.push_back(broadcastId);
}

void EntityManager::flushPendingBroadcasts() {
    for (uint32_t broadcastId : pendingBroadcasts) {
        EngineState::dispatchId++;
        VM::dispatchEvent(static_cast<uint32_t>(HatType::BROADCAST_RECEIVED), broadcastId, true, EngineState::dispatchId);
    }
    pendingBroadcasts.clear();
}

void EntityManager::resetEntityManager() {
    cloneCount = 0;
    freeInstances.clear();
    pendingClones.clear();
    pendingDeletions.clear();
    pendingBroadcasts.clear();
    blueprints.clear();
    transforms.clear();
    renderInfo.clear();
    blueprintIds.clear();
    activeInstances.clear();
    variables.clear();
    lists.clear();
    ttsData.clear();
    effects.clear();
    audio.clear();
    pen.clear();
    renderOrder.clear();
    spritesToClick.clear();
    layerOrderDirty = false;
    stageSprite = 0;
}

void EntityManager::flushPendingDeletions() {
    for (uint32_t id : pendingDeletions) {
        if (id >= activeInstances.size() || !activeInstances[id]) continue;
        if (!transforms[id].isClone() && cloneCount <= 0) continue;
        cloneCount--;

        activeInstances[id] = false;

        spritesToClick.erase(std::remove(spritesToClick.begin(), spritesToClick.end(), id), spritesToClick.end());

        VM::killAllThreadsOfInstance(id);

        SpeechManager *speechManager = Render::getSpeechManager();
        if (speechManager) speechManager->clearSpeech(id);

        renderOrder.erase(
            std::remove(renderOrder.begin(), renderOrder.end(), id),
            renderOrder.end());

        freeInstances.push_back(id);
    }

    pendingDeletions.clear();
    layerOrderDirty = true;
}

void EntityManager::reserve(int spriteAmount) {
    blueprints.reserve(spriteAmount);
    transforms.reserve(spriteAmount);
    renderInfo.reserve(spriteAmount);
    blueprintIds.reserve(spriteAmount);
    activeInstances.reserve(spriteAmount);
    variables.reserve(spriteAmount);
    lists.reserve(spriteAmount);
    ttsData.reserve(spriteAmount);
    effects.reserve(spriteAmount);
    audio.reserve(spriteAmount);
    pen.reserve(spriteAmount);
    renderOrder.reserve(spriteAmount);
}

uint32_t EntityManager::addOneEmptySprite() {
    uint32_t defId = blueprints.size();

    blueprints.push_back({});
    transforms.push_back({});
    renderInfo.push_back({});
    blueprintIds.push_back(defId);
    activeInstances.push_back(true);
    variables.push_back({});
    lists.push_back({});
    ttsData.push_back({});
    effects.push_back({});
    audio.push_back({});
    pen.push_back({});
    renderOrder.push_back(defId);
    return defId;
}

bool EntityManager::isColliding(CollisionMode mode, uint32_t instance1, uint32_t instance2) {
    switch (mode) {
    case CollisionMode::MOUSE: {
        return EngineState::accurateCollision
                   ? collision::pointInSprite(instance1, Input::mousePointer.x, Input::mousePointer.y)
                   : collision::pointInSpriteFast(instance1, Input::mousePointer.x, Input::mousePointer.y);
    }
    case CollisionMode::EDGE:
        return EngineState::accurateCollision
                   ? collision::spriteOnEdge(instance1)
                   : collision::spriteOnEdgeFast(instance1);

    case CollisionMode::SPRITE:
        return EngineState::accurateCollision
                   ? collision::spriteInSprite(instance1, instance2)
                   : collision::spriteInSpriteFast(instance1, instance2);
    }
    return false;
}