#include "../../input.hpp"
#include "../data/blueprint.hpp"
#include "../opcode_registers.hpp"

DEFINE_CUSTOM_PARSER("event_if_else", parseIfElse) {
    BytecodeChunk chunk;

    CompileResult condRes = ctx.compileInput("CONDITION");

    if (condRes.isConstant) {
        if (condRes.constantValue.asBoolean()) {
            const auto &inputs = ctx.blocksJson[ctx.currentBlock]["inputs"];
            if (inputs.contains(
                    "SUBSTACK")) {
                const auto &subData = inputs["SUBSTACK"];
                if (subData.is_array() && subData.size() >= 2 && subData[1].is_string()) {
                    std::string subStackStartId = subData[1].get<std::string>();
                    ctx.compileSequence(subStackStartId, chunk);
                }
            }
        } else {
            const auto &inputs = ctx.blocksJson[ctx.currentBlock]["inputs"];
            if (inputs.contains("SUBSTACK2")) {
                const auto &subData = inputs["SUBSTACK2"];
                if (subData.is_array() && subData.size() >= 2 && subData[1].is_string()) {
                    std::string subStackStartId = subData[1].get<std::string>();
                    ctx.compileSequence(subStackStartId, chunk);
                }
            }
        }
        return CompileResult::Dynamic(std::move(chunk));
    } else {
        chunk.append(std::move(condRes.chunk));
    }

    uint16_t elseJumpIdx = chunk.emitForwardJump(static_cast<uint16_t>(Opcode::JUMP_FWD_IF_FALSE));

    const auto &inputs = ctx.blocksJson[ctx.currentBlock]["inputs"];
    if (inputs.contains("SUBSTACK")) {
        const auto &subData = inputs["SUBSTACK"];
        if (subData.is_array() && subData.size() >= 2 && subData[1].is_string()) {
            std::string subStackStartId = subData[1].get<std::string>();
            ctx.compileSequence(subStackStartId, chunk);
        }
    }

    uint16_t endJumpIdx = chunk.emitForwardJump(static_cast<uint16_t>(Opcode::JUMP_FWD));

    chunk.patchForwardJump(elseJumpIdx);

    if (inputs.contains("SUBSTACK2")) {
        const auto &subData = inputs["SUBSTACK2"];
        if (subData.is_array() && subData.size() >= 2 && subData[1].is_string()) {
            std::string subStackStartId = subData[1].get<std::string>();
            ctx.compileSequence(subStackStartId, chunk);
        }
    }

    chunk.patchForwardJump(endJumpIdx);

    return CompileResult::Dynamic(std::move(chunk));
}

REGISTER_SIMPLE_HAT("event_whenflagclicked", HatType::FLAG_CLICKED)
// REGISTER_SIMPLE_HAT("control_start_as_a_clone", HatType::CLONE_START)
// REGISTER_SIMPLE_HAT("event_whenthisspriteclicked", HatType::THIS_SPRITE_CLICKED)
// REGISTER_SIMPLE_HAT("event_whenstageclicked", HatType::STAGE_CLICKED)

// DEFINE_HAT_PARSER("event_whenbroadcastreceived", broadcast_received) {
//     std::string broadcastName = ctx.resolveFieldValue("BROADCAST_OPTION");
//     uint16_t broadcastId = 0;
//
//     if (ctx.targetDef.broadcasts.count(broadcastName)) {
//         broadcastId = ctx.targetDef.broadcasts[broadcastName];
//     } else if (stageContext && stageContext->targetDef.broadcasts.count(broadcastName)) {
//         broadcastId = stageContext->targetDef.broadcasts[broadcastName];
//     }
//
//     return {HatType::BROADCAST_RECEIVED, broadcastId, true};
// }
//// Not that happy with this hashing here tho ...
// DEFINE_HAT_PARSER("event_whenkeypressed", key_pressed) {
//     std::string keyOption = ctx.resolveFieldValue("KEY_OPTION");
//     std::string key = Input::convertToKey(Value(keyOption), false);
//     std::transform(key.begin(), key.end(), key.begin(), ::tolower);
//     uint16_t keyId = static_cast<uint16_t>(std::hash<std::string>{}(key));
//     return {HatType::KEY_PRESSED, keyId, true};
// }