#pragma once

#include <cstdint>
#include <vector>

#include "data/blueprint.hpp"
#include "data/entity_components.hpp"

class EntityManager {
  private:
    static inline std::vector<uint32_t> freeInstances;
    static inline std::vector<uint32_t> pendingClones;
    static inline std::vector<uint32_t> pendingDeletions;
    static uint32_t allocateInstance(uint16_t defId);

  public:
    static inline bool dirtyPointer = true;
    static inline uint16_t cloneCount = 0;
    static inline std::vector<TargetDefinition> blueprints;

    static inline std::vector<SpriteTransform> transforms;
    static inline std::vector<RenderInfo> renderInfo;
    static inline std::vector<uint16_t> blueprintIds;
    static inline std::vector<bool> activeInstances;

    static inline std::vector<Variables> variables;
    static inline std::vector<Lists> lists;

    static inline std::vector<TTSData> ttsData;
    static inline std::vector<GraphicEffects> effects;
    static inline std::vector<AudioState> audio;
    static inline std::vector<PenState> pen;

    static inline uint32_t stageSprite;
    static inline TargetDefinition *stageBlueprint;
    static inline std::vector<uint32_t> renderOrder;
    static inline bool layerOrderDirty;

    static inline std::vector<uint32_t> spritesToClick;

    static uint32_t createInstance(uint16_t defId);

    static void queueClone(uint32_t sourceId);
    static void queueDeletion(uint32_t instanceId);

    static void syncLayerIndices();
    static void moveForward(uint32_t instanceId, uint32_t amount = 1);
    static void moveBackwards(uint32_t instanceId, uint32_t amount = 1);
    static void goToFrontLayer(uint32_t instanceId);
    static void goToBackLayer(uint32_t instanceId);

    static void queueBroadcast(uint32_t broadcastId);

    static void flushPendingClones();

    static void flushPendingDeletions();

    static void reserve(uint16_t spriteAmount, uint16_t cloneAmount);

    static uint32_t addOneEmptySprite();

    enum class CollisionMode {
        MOUSE,
        EDGE,
        SPRITE
    };

    static bool isColliding(CollisionMode mode, uint32_t instance1, uint32_t instance2);

    static void resetEntityManager();
};
