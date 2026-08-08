#include "../../entity_manager.hpp"
#include "../../vm/engine_state.hpp"
#include "../opcode_registers.hpp"
#include "../opcodes.hpp"
#include "blueprint.hpp"
#include "entity_manager.hpp"
#include <cstdint>

DEFINE_EXECUTION(PUSH_POS_INT) {
    TargetDefinition &def = EntityManager::blueprints[thread->defId];
    uint16_t posInt = def.bytecode[thread->pc++];
    thread->stack.emplace_back(Value(posInt));
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(PUSH_NEG_INT) {
    TargetDefinition &def = EntityManager::blueprints[thread->defId];
    int16_t negInt = def.bytecode[thread->pc++];
    thread->stack.emplace_back(Value(-negInt));
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(PUSH_FALSE) {
    thread->stack.emplace_back(Value(false));
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(PUSH_TRUE) {
    thread->stack.emplace_back(Value(true));
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(PUSH_CONST) {
    TargetDefinition &def = EntityManager::blueprints[thread->defId];
    uint16_t constIndex = def.bytecode[thread->pc++];
    thread->stack.emplace_back(ConstantPool::getCopy(constIndex));
    return BlockResult::CONTINUE;
}
