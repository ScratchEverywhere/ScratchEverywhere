#include "../../entity_manager.hpp"
#include "../../input.hpp"
#include "../../vm/vm.hpp"
#include "../data/blueprint.hpp"
#include "../opcode_registers.hpp"
#include "compiler_context.hpp"
#include "engine_state.hpp"
#include "scratch_engine.hpp"
#include "vm_types.hpp"

REGISTER_SIMPLE_HAT("event_whenflagclicked", HatType::FLAG_CLICKED)
REGISTER_SIMPLE_HAT("event_whenthisspriteclicked", HatType::THIS_SPRITE_CLICKED)
REGISTER_SIMPLE_HAT("event_whenstageclicked", HatType::STAGE_CLICKED)
DEFINE_HAT_PARSER("event_whenbackdropswitchesto", backdropSwitchesTo_parser) {
    std::string name = ctx.resolveFieldValue("BACKDROP");
    uint32_t backdropId = 0;
    for (uint32_t i = 0; i < ctx.targetDef.costumes.size(); i++) {
        if (ctx.targetDef.costumes[i].name == name) {
            backdropId = i;
            break;
        }
    }
    return {HatType::BACKDROP_SWITCHED, backdropId, true};
}

DEFINE_HAT_PARSER("event_whengreaterthan", parseGreaterThan) {
    std::string menu = ctx.resolveFieldValue("WHENGREATERTHANMENU");
    uint16_t sensorId = 0;
    if (menu == "LOUDNESS") sensorId = 1;
    return {HatType::GREATER_THAN, sensorId, true};
}

DEFINE_HAT_PARSER("event_whenkeypressed", key_pressed_parser) {
    std::string keyOption = ctx.resolveFieldValue("KEY_OPTION");
    Log::log("KEY_OPTION value: '" + keyOption + "'");
    std::string key = Input::convertToKey(Value(keyOption), false);
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    uint32_t keyId = ScratchEngine::fnv1a_32(key);
    return {HatType::KEY_PRESSED, keyId, true};
}

DEFINE_HAT_PARSER("event_whenbroadcastreceived", broadcast_received_parser) {
    std::string broadcastName = ctx.resolveFieldValue("BROADCAST_OPTION");
    uint16_t broadcastId = 0;
    // broadcasts are normally stored at the stage
    if (ctx.targetDef.broadcasts.count(broadcastName) > 0) {
        broadcastId = ctx.targetDef.broadcasts[broadcastName];
    } else if (stageContext && stageContext->targetDef.broadcasts.count(broadcastName) > 0) {
        broadcastId = stageContext->targetDef.broadcasts[broadcastName];
    }

    return {HatType::BROADCAST_RECEIVED, broadcastId, true};
}

DEFINE_CUSTOM_PARSER("event_broadcast", event_broadcast_parser) {
    BytecodeChunk chunk;
    CompileResult broadcastInput = ctx.compileInput("BROADCAST_INPUT");
    if (broadcastInput.isConstant) {
        uint16_t broadcastId = 0;
        if (ctx.targetDef.broadcasts.count(broadcastInput.constantValue.asString()) > 0) {
            broadcastId = ctx.targetDef.broadcasts[broadcastInput.constantValue.asString()];
        } else if (stageContext && stageContext->targetDef.broadcasts.count(broadcastInput.constantValue.asString()) > 0) {
            broadcastId = stageContext->targetDef.broadcasts[broadcastInput.constantValue.asString()];
        } else {
            Log::log("Broadcast not found: " + broadcastInput.constantValue.asString());
            chunk.emitOpcode(static_cast<uint16_t>(Opcode::YIELD));
            return CompileResult::Dynamic(std::move(chunk));
        }
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::event_broadcast));
        chunk.emit16(broadcastId);
    } else {
        chunk.append(std::move(broadcastInput.chunk));
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::event_broadcast_dynamic));
    }
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_CUSTOM_PARSER("event_broadcastandwait", event_broadcastandwait_parser) {
    BytecodeChunk chunk;
    CompileResult broadcastInput = ctx.compileInput("BROADCAST_OPTION");
    if (broadcastInput.isConstant) {
        uint16_t broadcastId = 0;
        if (ctx.targetDef.broadcasts.count(broadcastInput.constantValue.asString()) > 0) {
            broadcastId = ctx.targetDef.broadcasts[broadcastInput.constantValue.asString()];
        } else if (stageContext && stageContext->targetDef.broadcasts.count(broadcastInput.constantValue.asString()) > 0) {
            broadcastId = stageContext->targetDef.broadcasts[broadcastInput.constantValue.asString()];
        } else {
            Log::log("Broadcast not found: " + broadcastInput.constantValue.asString());
            chunk.emitOpcode(static_cast<uint16_t>(Opcode::YIELD));
            return CompileResult::Dynamic(std::move(chunk));
        }
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::event_broadcastandwait));
        chunk.emit16(broadcastId);
    } else {
        chunk.append(std::move(broadcastInput.chunk));
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::event_broadcastandwait_dynamic));
    }
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_EXECUTION(event_broadcast) {
    uint16_t broadcastId = thread->definition->bytecode[thread->pc++];
    VM::dispatchEvent(static_cast<uint32_t>(HatType::BROADCAST_RECEIVED), broadcastId, true, DISPATCH_ID_NONE);
    return BlockResult::YIELD_NEXT;
}

DEFINE_EXECUTION(event_broadcast_dynamic) {
    std::string broadcast = thread->stack.back().asString();
    thread->stack.pop_back();
    uint16_t broadcastId = 0;
    if (EntityManager::stageBlueprint->broadcasts.count(broadcast)) {
        broadcastId = EntityManager::stageBlueprint->broadcasts[broadcast];
    } else {
        Log::log("Broadcast not found: " + broadcast);
        return BlockResult::YIELD_NEXT;
    }
    VM::dispatchEvent(static_cast<uint32_t>(HatType::BROADCAST_RECEIVED), broadcastId, true, DISPATCH_ID_NONE);
    return BlockResult::YIELD_NEXT;
}

DEFINE_EXECUTION(event_broadcastandwait) {
    uint16_t broadcastId = thread->definition->bytecode[thread->pc++];
    uint32_t dispatchId = ++EngineState::dispatchId;
    thread->waitHandle = dispatchId;
    thread->state = ThreadState::WAITING_FOR_BROADCAST;
    VM::dispatchEvent(static_cast<uint32_t>(HatType::BROADCAST_RECEIVED), broadcastId, true, dispatchId);
    return BlockResult::YIELD_NEXT;
}
DEFINE_EXECUTION(event_broadcastandwait_dynamic) {
    std::string broadcast = thread->stack.back().asString();
    thread->stack.pop_back();
    uint16_t broadcastId = 0;
    if (EntityManager::stageBlueprint->broadcasts.count(broadcast)) {
        broadcastId = EntityManager::stageBlueprint->broadcasts[broadcast];
    } else {
        Log::log("Broadcast not found: " + broadcast);
        return BlockResult::YIELD_NEXT;
    }
    uint32_t dispatchId = ++EngineState::dispatchId;
    thread->waitHandle = dispatchId;
    thread->state = ThreadState::WAITING_FOR_BROADCAST;
    VM::dispatchEvent(static_cast<uint32_t>(HatType::BROADCAST_RECEIVED), broadcastId, true, dispatchId);
    return BlockResult::YIELD_NEXT;
}

DEFINE_EXECUTION(event_when_larger_then) {
    Value dropdownValue = thread->stack.back();
    Value value = thread->stack[thread->stack.size() - 2];
    Value lastValue = thread->stack[thread->stack.size() - 3];

    bool result = dropdownValue > value;
    if (result && !(lastValue == value)) {
        thread->stack[thread->stack.size() - 3] = value;
    } else {
        result = false;
    }
    thread->stack.pop_back();
    thread->stack.pop_back();
    thread->stack.emplace_back(result);
    return BlockResult::YIELD_NEXT;
}

// Events broadcasts