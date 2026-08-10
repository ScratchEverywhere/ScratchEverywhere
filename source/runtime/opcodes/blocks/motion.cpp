#include "../../data/entity_components.hpp"
#include "../../entity_manager.hpp"
#include "../../systems/sprite_system.hpp"
#include "../opcode_registers.hpp"
#include "../opcodes.hpp"
#include "engine_state.hpp"

REGISTER_STANDARD_PARSER("motion_movesteps", motion_movesteps, "STEPS")
DEFINE_EXECUTION(motion_movesteps) {
    Log::log("motion_movesteps called!");
    const Value &stepsValue = thread->stack.back();
    Log::log("steps: " + stepsValue.asString());
    const SpriteTransform &transform = EntityManager::transforms[thread->instanceId];
    const double steps = stepsValue.asDouble();
    const double angle = Math::degreesToRadians(90.0 - transform.direction);
    SpriteSystem::gotoXY(thread->instanceId, transform.x + std::cos(angle) * steps, transform.y + std::sin(angle) * steps);
    thread->stack.pop_back();
    return BlockResult::YIELD_NEXT; // Every block that changes the sprite's appearance or position should yield. (Just like the original runtime)
}
