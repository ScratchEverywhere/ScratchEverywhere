#include "../../entity_manager.hpp"
#include "../../vm/engine_state.hpp"
#include "../opcode_registers.hpp"
#include "../opcodes.hpp"
#include "blueprint.hpp"
#include "entity_manager.hpp"
#include <cstdint>

DEFINE_EXECUTION(YIELD) {
    return BlockResult::YIELD_NEXT;
}

DEFINE_EXECUTION(PUSH_POS_INT) {
    uint16_t posInt = thread->definition->bytecode[thread->pc++];
    thread->stack.emplace_back(Value(posInt));
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(PUSH_NEG_INT) {
    int16_t negInt = thread->definition->bytecode[thread->pc++];
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
    uint16_t constIndex = thread->definition->bytecode[thread->pc++];
    thread->stack.emplace_back(ConstantPool::getCopy(constIndex));
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(RETURN) {
    if (thread->callStack.empty()) {
        thread->state = ThreadState::DEAD;
        return BlockResult::YIELD_SAME;
    }

    CallFrame frame = thread->callStack.back();
    thread->callStack.pop_back();

    thread->stack.resize(frame.stackBase - frame.argCount);

    thread->pc = frame.returnPC;
    thread->isWarp = frame.previousWarpState;
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(JUMP_FWD) {
    uint16_t offset = thread->definition->bytecode[thread->pc++];
    thread->pc += offset;
    return BlockResult::YIELD_NEXT;
}

DEFINE_EXECUTION(JUMP_BACK) {
    uint16_t offset = thread->definition->bytecode[thread->pc];
    thread->pc -= offset + 1;
    // Log::log("JUMP BACK! " + std::to_string(thread->pc));
    return BlockResult::YIELD_NEXT;
}

DEFINE_EXECUTION(JUMP_FWD_IF_FALSE) {
    uint16_t offset = thread->definition->bytecode[thread->pc++];
    Value &condition = thread->stack.back();
    if (!condition.asBoolean()) {
        thread->pc += offset;
    }
    thread->stack.pop_back();
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(JUMP_BACK_IF_FALSE) {
    uint16_t offset = thread->definition->bytecode[thread->pc];
    Value &condition = thread->stack.back();
    if (!condition.asBoolean()) {
        thread->pc -= offset + 1;
    }
    thread->stack.pop_back();
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(JUMP_ABS_32) {
    uint32_t target = thread->definition->bytecode[thread->pc + 1] | (thread->definition->bytecode[thread->pc + 2] << 16);
    thread->pc = target;
    return BlockResult::YIELD_NEXT;
}