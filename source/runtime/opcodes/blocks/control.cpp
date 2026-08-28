#include "../../entity_manager.hpp"
#include "../../vm/vm.hpp"
#include "../opcode_registers.hpp"
#include "../opcodes.hpp"
#include "blueprint.hpp"
#include "bytecode_chunk.hpp"
#include "compiler_context.hpp"
#include "entity_manager.hpp"
#include <cstdint>
#include <string>

DEFINE_CUSTOM_PARSER("control_if", control_if_parser) {
    BytecodeChunk chunk;
    CompileResult condRes = ctx.compileInput("CONDITION");
    if (condRes.isConstant) {
        if (condRes.constantValue.asBoolean()) {
            const auto &inputs = ctx.blocksJson[ctx.currentBlock]["inputs"];
            if (inputs.contains("SUBSTACK")) {
                const auto &subData = inputs["SUBSTACK"];
                if (subData.is_array() && subData.size() >= 2 && subData[1].is_string()) {
                    std::string subStackStartId = subData[1].get<std::string>();
                    if (ctx.blocksJson.contains(subStackStartId)) {
                        ctx.compileSequence(subStackStartId, chunk);
                    }
                }
            }
        }
        return CompileResult::Dynamic(std::move(chunk));
    }
    chunk.append(std::move(condRes.chunk));

    uint16_t elseJumpIdx = chunk.emitForwardJump(static_cast<uint16_t>(Opcode::JUMP_FWD_IF_FALSE));

    const auto &inputs = ctx.blocksJson[ctx.currentBlock]["inputs"];
    if (inputs.contains("SUBSTACK")) {
        const auto &subData = inputs["SUBSTACK"];
        if (subData.is_array() && subData.size() >= 2 && subData[1].is_string()) {
            std::string subStackStartId = subData[1].get<std::string>();
            if (ctx.blocksJson.contains(subStackStartId)) {
                ctx.compileSequence(subStackStartId, chunk);
            }
        }
    }

    chunk.patchForwardJump(elseJumpIdx);

    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_CUSTOM_PARSER("control_if_else", parseIfElse) {
    BytecodeChunk chunk;

    CompileResult condRes = ctx.compileInput("CONDITION");

    if (condRes.isConstant) {
        if (condRes.constantValue.asBoolean()) {
            const auto &inputs = ctx.blocksJson[ctx.currentBlock]["inputs"];
            if (inputs.contains("SUBSTACK")) {
                const auto &subData = inputs["SUBSTACK"];
                if (subData.is_array() && subData.size() >= 2 && subData[1].is_string()) {
                    std::string subStackStartId = subData[1].get<std::string>();
                    if (ctx.blocksJson.contains(subStackStartId)) {
                        ctx.compileSequence(subStackStartId, chunk);
                    }
                }
            }
        } else {
            const auto &inputs = ctx.blocksJson[ctx.currentBlock]["inputs"];
            if (inputs.contains("SUBSTACK2")) {
                const auto &subData = inputs["SUBSTACK2"];
                if (subData.is_array() && subData.size() >= 2 && subData[1].is_string()) {
                    std::string subStackStartId = subData[1].get<std::string>();
                    if (ctx.blocksJson.contains(subStackStartId)) {
                        ctx.compileSequence(subStackStartId, chunk);
                    }
                }
            }
        }
        return CompileResult::Dynamic(std::move(chunk));
    }
    chunk.append(std::move(condRes.chunk));

    uint16_t elseJumpIdx = chunk.emitForwardJump(static_cast<uint16_t>(Opcode::JUMP_FWD_IF_FALSE));

    const auto &inputs = ctx.blocksJson[ctx.currentBlock]["inputs"];
    if (inputs.contains("SUBSTACK")) {
        const auto &subData = inputs["SUBSTACK"];
        if (subData.is_array() && subData.size() >= 2 && subData[1].is_string()) {
            std::string subStackStartId = subData[1].get<std::string>();
            if (ctx.blocksJson.contains(subStackStartId)) {
                ctx.compileSequence(subStackStartId, chunk);
            }
        }
    }

    uint16_t endJumpIdx = chunk.emitForwardJump(static_cast<uint16_t>(Opcode::JUMP_FWD));

    chunk.patchForwardJump(elseJumpIdx);

    if (inputs.contains("SUBSTACK2")) {
        const auto &subData = inputs["SUBSTACK2"];
        if (subData.is_array() && subData.size() >= 2 && subData[1].is_string()) {
            std::string subStackStartId = subData[1].get<std::string>();
            if (ctx.blocksJson.contains(subStackStartId)) {
                ctx.compileSequence(subStackStartId, chunk);
            }
        }
    }

    chunk.patchForwardJump(endJumpIdx);

    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_CUSTOM_PARSER("control_create_clone_of", parseRepeat) {
    BytecodeChunk chunk;
    CompileResult cloneRes = ctx.compileInput("CLONE_OPTION");
    if (cloneRes.isConstant) {
        std::string input = cloneRes.constantValue.asString();
        if (input == "_myself_") {
            chunk.emitOpcode(static_cast<uint16_t>(Opcode::control_create_clone_of_me));
            return CompileResult::Dynamic(std::move(chunk));
        } else {
            for (uint16_t blueprintId = 0; blueprintId < EntityManager::blueprints.size(); blueprintId++) {
                if (EntityManager::blueprints[blueprintId].name == input) {
                    chunk.emitOpcode(static_cast<uint16_t>(Opcode::control_create_clone_of_other));
                    chunk.emit16(blueprintId);
                    return CompileResult::Dynamic(std::move(chunk));
                }
            }
            chunk.emitPushConstant(Value(input));
        }
    } else {
        chunk.append(std::move(cloneRes.chunk));
    }
    chunk.emitOpcode(static_cast<uint16_t>(Opcode::control_create_clone_of_other));
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_CUSTOM_PARSER("control_create_clone_of_menu", clone_of_menu_parser) {
    return CompileResult::Constant(Value(ctx.resolveFieldValue("CLONE_OPTION")));
}

DEFINE_EXECUTION(control_create_clone_of) {
    std::string spriteName = thread->stack.back().asString();
    thread->stack.pop_back();
    if (spriteName == "_myself_") {
        EntityManager::queueClone(thread->defId);
        return BlockResult::CONTINUE;
    }
    for (uint16_t blueprintId = 0; blueprintId < EntityManager::blueprints.size(); blueprintId++) {
        if (EntityManager::blueprints[blueprintId].name == spriteName) {
            EntityManager::queueClone(blueprintId);
            break;
        }
    }
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(control_create_clone_of_me) {
    EntityManager::queueClone(thread->defId);
    return BlockResult::CONTINUE;
}
DEFINE_EXECUTION(control_create_clone_of_other) {
    uint16_t blueprintId = thread->definition->bytecode[thread->pc++];
    EntityManager::queueClone(blueprintId);
    return BlockResult::CONTINUE;
}

REGISTER_STANDARD_PARSER("control_delete_this_clone", control_delete_this_clone)

DEFINE_EXECUTION(control_delete_this_clone) {
    EntityManager::queueDeletion(thread->instanceId);
    return BlockResult::YIELD_SAME;
}

DEFINE_CUSTOM_PARSER("control_stop", stop_parser) {
    BytecodeChunk chunk;
    std::string input = ctx.resolveFieldValue("STOP_OPTION");
    if (input == "all") {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::control_stop_all));
    } else if (input == "this script") {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::RETURN));
    } else if (input == "this sprite") {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::control_stop_other_scripts));
    }
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_EXECUTION(control_stop_all) {
    return BlockResult::YIELD_SAME;
}

DEFINE_EXECUTION(control_stop_other_scripts) {
    VM::killAllThreadsOfInstance(thread->instanceId);
    thread->state = ThreadState::RUNNING;
    return BlockResult::CONTINUE;
}

REGISTER_SIMPLE_HAT("control_start_as_clone", HatType::CLONE_START)

DEFINE_CUSTOM_PARSER("control_forever", parseForever) {
    BytecodeChunk chunk;
    const auto &inputs = ctx.blocksJson[ctx.currentBlock]["inputs"];
    const auto &subData = inputs["SUBSTACK"];
    if (!inputs.contains("SUBSTACK") || !subData.is_array() || subData.size() < 2 ||
        !subData[1].is_string() || !ctx.blocksJson.contains(subData[1].get<std::string>())) {
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::RETURN));
        return CompileResult::Dynamic(chunk);
    }
    std::string subStackStartId = subData[1].get<std::string>();
    ctx.compileSequence(subStackStartId, chunk);
    chunk.emitOpcode(static_cast<uint16_t>(Opcode::YIELD));
    chunk.emitBackwardJump(static_cast<uint16_t>(chunk.code.size()));

    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_CUSTOM_PARSER("control_wait_until", parseWaitUntil) {
    BytecodeChunk chunk;
    CompileResult condRes = ctx.compileInput("CONDITION");

    if (condRes.isConstant) {
        if (condRes.constantValue.asBoolean()) {
            return CompileResult::Dynamic(std::move(chunk));
        } else {
            chunk.emitOpcode(static_cast<uint16_t>(Opcode::RETURN));
            return CompileResult::Dynamic(std::move(chunk));
        }
    }
    chunk.append(std::move(condRes.chunk));

    size_t whileJumpIdx = chunk.emitForwardJump(static_cast<uint16_t>(Opcode::JUMP_FWD_IF_FALSE));

    chunk.emitBackwardJump(static_cast<uint16_t>(chunk.code.size()));

    chunk.patchForwardJump(whileJumpIdx);

    return CompileResult::Dynamic(std::move(chunk));
}

REGISTER_STANDARD_PARSER("control_wait", control_wait, "DURATION")
DEFINE_EXECUTION(control_wait) {
    thread->sleepTimer = thread->stack.back().asDouble();
    thread->stack.pop_back();
    thread->state = ThreadState::WAITING_FOR_TIME;
    return BlockResult::YIELD_NEXT;
}

DEFINE_CUSTOM_PARSER("control_repeat", control_repeat_parser) {
    BytecodeChunk chunk;
    // Log::log("control_repeat parser");
    CompileResult countRes = ctx.compileInput("TIMES");
    if (countRes.isConstant) chunk.emitPushConstant(countRes.constantValue);
    else chunk.append(std::move(countRes.chunk));
    // Log::log("control_repeat parser count done");
    // for (auto &byte : chunk.code) {
    //     Log::log("\t" + std::to_string(byte));
    // }
    uint16_t jumpBackSkip = chunk.code.size();
    // Log::log(std::to_string(jumpBackSkip));
    chunk.emitOpcode(static_cast<uint16_t>(Opcode::control_repeat));
    size_t whileJumpIdx = chunk.emitForwardJump(static_cast<uint16_t>(Opcode::JUMP_FWD_IF_FALSE));
    const auto &inputs = ctx.blocksJson[ctx.currentBlock]["inputs"];
    const auto &subData = inputs["SUBSTACK"];
    if (inputs.contains("SUBSTACK") && subData.is_array() && subData.size() >= 2 &&
        subData[1].is_string() && ctx.blocksJson.contains(subData[1].get<std::string>())) {
        std::string subStackStartId = subData[1].get<std::string>();
        ctx.compileSequence(subStackStartId, chunk);
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::YIELD));
    }
    // Log::log("control_repeat parser substack done");
    // for (auto &byte : chunk.code) {
    //     Log::log("\t" + std::to_string(byte));
    // }
    chunk.emitBackwardJump(static_cast<uint16_t>(chunk.code.size() - jumpBackSkip));
    // Log::log("control_repeat parser jump back done");
    // for (auto &byte : chunk.code) {
    //     Log::log("\t" + std::to_string(byte));
    // }
    chunk.patchForwardJump(whileJumpIdx);
    // Log::log("control_repeat parser patch forward done");
    // for (auto &byte : chunk.code) {
    // Log::log("\t" + std::to_string(byte));
    // }
    return CompileResult::Dynamic(std::move(chunk));
}
DEFINE_EXECUTION(control_repeat) {
    Value &val = thread->stack.back();
    // Log::log("control_repeat stack size " + std::to_string(thread->stack.size()));
    // for (auto &val : thread->stack) {
    //    Log::log(val.asString() + " ");
    //
    val = Value(std::round(val.asDouble()) - 1);
    // Log::log("control_repeat val " + val.asString());
    if (val.asDouble() < 0) {
        // Log::log("control_repeat val < 0");
        thread->stack.pop_back();
        thread->stack.emplace_back(false);
        return BlockResult::CONTINUE;
    }
    // Log::log("control_repeat val >= 0");
    thread->stack.emplace_back(true);
    return BlockResult::CONTINUE;
}

DEFINE_CUSTOM_PARSER("control_while", while_parser) {
    BytecodeChunk chunk;
    CompileResult condition = ctx.compileInput("CONDITION");
    if (condition.isConstant) {
        if (condition.constantValue.asBoolean()) {
            CompileResult res = ParserRegistry::getParserMap()["control_forever"](ctx);
            return CompileResult::Dynamic(std::move(res.chunk));
        } else {
            return CompileResult::Dynamic({});
        }
    }
    chunk.append(std::move(condition.chunk));
    size_t whileJumpIdx = chunk.emitForwardJump(static_cast<uint16_t>(Opcode::JUMP_FWD_IF_FALSE));
    const auto &inputs = ctx.blocksJson[ctx.currentBlock]["inputs"];
    const auto &subData = inputs["SUBSTACK"];
    if (inputs.contains("SUBSTACK") && subData.is_array() && subData.size() >= 2 &&
        subData[1].is_string() && ctx.blocksJson.contains(subData[1].get<std::string>())) {
        std::string subStackStartId = subData[1].get<std::string>();
        ctx.compileSequence(subStackStartId, chunk);
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::YIELD));
    }
    chunk.emitBackwardJump(static_cast<uint16_t>(chunk.code.size()));
    chunk.patchForwardJump(whileJumpIdx);
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_CUSTOM_PARSER("control_repeat_until", repeat_until_parser) {
    BytecodeChunk chunk;
    CompileResult condition = ctx.compileInput("CONDITION");
    if (condition.isConstant) {
        if (!condition.constantValue.asBoolean()) {
            CompileResult res = ParserRegistry::getParserMap()["control_forever"](ctx);
            return CompileResult::Dynamic(std::move(res.chunk));
        } else {
            return CompileResult::Dynamic({});
        }
    }
    chunk.append(std::move(condition.chunk));

    // or we just use one instruction instead of 2? smth like JUMP_FWD_IF_TRUE?
    chunk.emitOpcode(static_cast<uint16_t>(Opcode::operator_not));
    size_t whileJumpIdx = chunk.emitForwardJump(static_cast<uint16_t>(Opcode::JUMP_FWD_IF_FALSE));

    const auto &inputs = ctx.blocksJson[ctx.currentBlock]["inputs"];
    const auto &subData = inputs["SUBSTACK"];
    if (inputs.contains("SUBSTACK") && subData.is_array() && subData.size() >= 2 &&
        subData[1].is_string() && ctx.blocksJson.contains(subData[1].get<std::string>())) {
        std::string subStackStartId = subData[1].get<std::string>();
        ctx.compileSequence(subStackStartId, chunk);
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::YIELD));
    }
    chunk.emitBackwardJump(static_cast<uint16_t>(chunk.code.size()));
    chunk.patchForwardJump(whileJumpIdx);
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_CUSTOM_PARSER("control_for_each", for_each_parser) {
    BytecodeChunk chunk;
    CompileResult upperBound = ctx.compileInput("VALUE");
    if (upperBound.isConstant) chunk.emitPushConstant(upperBound.constantValue);
    else chunk.append(std::move(upperBound.chunk));
    std::string variable = ctx.resolveFieldValue("VARIABLE");
    bool counting = false;
    uint16_t varid = 0;
    if (ctx.targetDef.variables.count(variable) > 0) {
        counting = true;
        varid = ctx.targetDef.variables[variable];

        chunk.emitPushConstant(Value(0.0));
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_setvariableto_private));
        chunk.emit16(varid);
    } else if (stageContext->targetDef.variables.count(variable) > 0) {
        counting = true;
        varid = stageContext->targetDef.variables[variable];

        chunk.emitPushConstant(Value(0.0));
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_setvariableto_public));
        chunk.emit16(varid);
    }

    uint16_t jumpBackSkip = chunk.code.size();

    chunk.emitOpcode(static_cast<uint16_t>(Opcode::control_repeat));
    if (counting) {
        chunk.emitPushConstant(Value(1.0));
        if (varid < ctx.targetDef.variables.size()) {
            chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_changevariableby_private));
            chunk.emit16(varid);
        } else {
            chunk.emitOpcode(static_cast<uint16_t>(Opcode::data_changevariableby_public));
            chunk.emit16(varid - ctx.targetDef.variables.size());
        }
    }
    size_t whileJumpIdx = chunk.emitForwardJump(static_cast<uint16_t>(Opcode::JUMP_FWD_IF_FALSE));
    const auto &inputs = ctx.blocksJson[ctx.currentBlock]["inputs"];
    const auto &subData = inputs["SUBSTACK"];
    if (inputs.contains("SUBSTACK") && subData.is_array() && subData.size() >= 2 &&
        subData[1].is_string() && ctx.blocksJson.contains(subData[1].get<std::string>())) {
        std::string subStackStartId = subData[1].get<std::string>();
        ctx.compileSequence(subStackStartId, chunk);
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::YIELD));
    }
    chunk.emitBackwardJump(static_cast<uint16_t>(chunk.code.size() - jumpBackSkip));
    chunk.patchForwardJump(whileJumpIdx);
    return CompileResult::Dynamic(std::move(chunk));
}

// DEFINE_EXECUTION(control_for_each) {
//     Value &upperBound = thread->stack.back();
//     upperBound = Value(std::round(upperBound.asDouble() - 1));
//     if (upperBound.asDouble() < 0) {
//         thread->stack.pop_back();
//         thread->stack.emplace_back(false);
//         return BlockResult::CONTINUE;
//     }
//     thread->stack.emplace_back(true);
//     return BlockResult::CONTINUE;
// }
