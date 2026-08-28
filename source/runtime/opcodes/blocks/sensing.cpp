#include "../../entity_manager.hpp"
#include "../../vm/engine_state.hpp"
#include "../opcode_registers.hpp"
#include "../opcodes.hpp"
#include "blueprint.hpp"
#include "entity_manager.hpp"
#include <cstdint>

REGISTER_STANDARD_PARSER("sensing_timer", sensing_timer)
DEFINE_EXECUTION(sensing_timer) {
    thread->stack.emplace_back(EngineState::timer.getTimeMs() / 1000.0);
    return BlockResult::CONTINUE;
}

REGISTER_STANDARD_PARSER("sensing_loudness", sensing_loudness)
DEFINE_EXECUTION(sensing_loudness) {
    thread->stack.emplace_back(0); // just push a 0 idk
    return BlockResult::CONTINUE;
}
