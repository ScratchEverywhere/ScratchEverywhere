#include <cstdint>

#include "../entity_manager.hpp"
#include "../opcodes/opcode_registers.hpp"
#include "../vm/engine_state.hpp"
#include "bytecode_chunk.hpp"
#include "compiler_context.hpp"
#include "entity_components.hpp"

CompileResult CompilerContext::compileExpression(const std::string blockId) {
    if (!blocksJson.contains(blockId))
        return CompileResult::Constant(Value());

    auto old = currentBlock;
    currentBlock = blockId;

    auto &block = blocksJson[blockId];

    auto &parserMap = ParserRegistry::getParserMap();
    if (parserMap.count(block["opcode"]) == 0) {
        currentBlock = old;
        return CompileResult::Constant(Value());
    }

    CompileResult res = parserMap[block["opcode"]](*this);

    currentBlock = old;

    return res;
}

void CompilerContext::compileSequence(std::string firstStatement, BytecodeChunk &outChunk) {
    std::string nextId = firstStatement;
    std::unordered_map<std::string, ParserHandler> &parserMap = ParserRegistry::getParserMap();

    while (!nextId.empty()) {
        if (!blocksJson.contains(nextId)) break;
        currentBlock = nextId;

        const auto &blockJson = blocksJson[currentBlock];
        if (!blockJson.contains("opcode")) break;

        std::string opcode = blockJson["opcode"].get<std::string>();

        if (parserMap.count(opcode) > 0) {
            CompileResult res = parserMap[opcode](*this);

            if (!res.isConstant) {
                outChunk.append(std::move(res.chunk));
            }
        }

        if (blockJson.contains("next") && blockJson["next"].is_string()) {
            nextId = blockJson["next"].get<std::string>();
        } else {
            break;
        }
    }
}

CompileResult CompilerContext::compileInput(const std::string &inputName) {
    if (!blocksJson[currentBlock]["inputs"].contains(inputName)) {
        Log::log("[CompilerContext] No input found for " + inputName + " in block " + currentBlock);
        return CompileResult::Constant(Value());
    }
    const nlohmann::json block = blocksJson[currentBlock];
    const nlohmann::json &input = block["inputs"][inputName];
    if (!input.is_array() || input.size() < 2 || !input[0].is_number()) {
        Log::log("[CompilerContext] Invalid input for " + inputName + " in block " + currentBlock);
        return CompileResult::Constant(Value());
    }
    const int type = input[0];
    const nlohmann::json &inputValue = input[1];

    Log::log("[CompilerContext] Input " + inputName + " of type " + std::to_string(type) + " in block " + currentBlock);

    if (type == 1) {
        if (inputValue.is_array()) {
            Value val = Value::fromJson(inputValue);
            Log::log("[CompilerContext] Compiled " + inputName + " in block " + currentBlock + " to constant: " + val.asString());
            return CompileResult::Constant(val);
        } else {
            return compileExpression(inputValue.get<std::string>());
        }
    } else if (type == 2 || type == 3) {
        if (inputValue.is_array()) {
            if (inputValue.size() <= 2) return CompileResult::Constant(Value()); // Different Fallback?
            std::string id = inputValue[2].get<std::string>();
            if (inputValue[0].get<int>() == 12) {
                // Variable
                BytecodeChunk chunk;
                if (targetDef.variables.count(id)) {
                    uint16_t variableId = targetDef.variables[id];
                    chunk.emitOpcode(static_cast<uint16_t>(Opcode::PUSH_PRI_VAR));
                    chunk.emit16(variableId);
                } else if (stageContext->targetDef.variables.count(id)) {
                    uint16_t variableId = stageContext->targetDef.variables[id];
                    chunk.emitOpcode(static_cast<uint16_t>(Opcode::PUSH_PUB_VAR));
                    chunk.emit16(variableId);
                } else {
                    return CompileResult::Constant(Value());
                }
                return CompileResult::Dynamic(chunk);
            } else if (inputValue[0].get<int>() == 13) {
                // List
                BytecodeChunk chunk;
                if (targetDef.lists.count(id)) {
                    uint16_t listId = targetDef.lists[id];
                    chunk.emitOpcode(static_cast<uint16_t>(Opcode::PUSH_PRI_LIST));
                    chunk.emit16(listId);
                } else if (stageContext->targetDef.lists.count(id)) {
                    uint16_t listId = stageContext->targetDef.lists[id];
                    chunk.emitOpcode(static_cast<uint16_t>(Opcode::PUSH_PUB_LIST));
                    chunk.emit16(listId);
                } else {
                    return CompileResult::Constant(Value());
                }
                return CompileResult::Dynamic(chunk);
            } else {
                Value val = Value::fromJson(inputValue);
                Log::log("[CompilerContext] Compiled " + inputName + " in block " + currentBlock + " to constant: " + val.asString());
                return CompileResult::Constant(val);
            }
        }
        return compileExpression(inputValue.get<std::string>());
    }

    return CompileResult::Constant(Value());
}
std::string CompilerContext::resolveFieldValue(const std::string &name) const {
    auto fields = blocksJson.value(currentBlock, nlohmann::json::object()).value("fields", nlohmann::json::object());
    auto params = blocksJson.value("params", nlohmann::json::object());
    if (!fields.contains(name) || !fields[name].is_array() || fields[name].empty()) {
        if (params.contains(name)) {
            return params[name][0].get<std::string>();
        }
        return "";
    }
    return fields[name][0].get<std::string>();
}

void CompilerContext::parseProcedures() {
    for (const auto &[id, data] : blocksJson.items()) {
        if (!data.is_object()) continue;
        if (!data.contains("opcode")) continue;
        if (data["opcode"].get<std::string>() != "procedures_definition") continue;

        std::string proccode = "";
        std::vector<std::string> argIds;
        bool warp = false;

        if (data.contains("inputs") && data["inputs"].contains("custom_block")) {
            const auto &customBlockInput = data["inputs"]["custom_block"];
            if (customBlockInput.is_array() && customBlockInput.size() >= 2 && customBlockInput[1].is_string()) {
                std::string protoId = customBlockInput[1].get<std::string>();
                if (blocksJson.contains(protoId)) {
                    const auto &protoBlock = blocksJson[protoId];
                    if (protoBlock.contains("mutation") && protoBlock["mutation"].is_object()) {
                        const auto &mutation = protoBlock["mutation"];
                        if (mutation.contains("proccode") && mutation["proccode"].is_string()) {
                            proccode = mutation["proccode"].get<std::string>();
                        }
                        if (mutation.contains("warp")) {
                            if (mutation["warp"].is_boolean()) {
                                warp = mutation["warp"].get<bool>();
                            } else if (mutation["warp"].is_string()) {
                                warp = (mutation["warp"].get<std::string>() == "true");
                            }
                        }
                        if (mutation.contains("argumentids")) {
                            if (mutation["argumentids"].is_string()) {
                                std::string argStr = mutation["argumentids"].get<std::string>();
                                if (!argStr.empty()) {
                                    auto parsedArgs = nlohmann::json::parse(argStr, nullptr, false);
                                    if (parsedArgs.is_array()) {
                                        for (const auto &item : parsedArgs) {
                                            if (item.is_string()) argIds.push_back(item.get<std::string>());
                                        }
                                    }
                                }
                            } else if (mutation["argumentids"].is_array()) {
                                for (const auto &item : mutation["argumentids"]) {
                                    if (item.is_string()) argIds.push_back(item.get<std::string>());
                                }
                            }
                        }
                    }
                }
            }
        }

        if (proccode.empty()) continue;

        procedureTable.addBlockName(proccode, id);
        procedureTable.getOrCreateSlot(proccode);
        ProcedureCompileInfo &info = procedureTable.proccodeToInfo[proccode];
        info.warp = warp;
        info.argumentIdsInOrder = argIds;
    }
}

void CompilerContext::parseScripts() {
    std::vector<BytecodeChunk::ProcedureCallPatch> allPatches;

    parseProcedures();

    for (const auto &proccode : procedureTable.worklist) {
        auto &info = procedureTable.proccodeToInfo[proccode];
        if (info.compiled) continue;

        std::string defBlockId = "";
        for (const auto &[bId, pCode] : procedureTable.blockIdToProccode) {
            if (pCode == proccode) {
                defBlockId = bId;
                break;
            }
        }
        if (defBlockId.empty() || !blocksJson.contains(defBlockId)) continue;

        uint32_t address = static_cast<uint32_t>(targetDef.bytecode.size());
        procedureTable.registerAddress(proccode, address);

        const auto &defBlock = blocksJson[defBlockId];
        std::string firstStatement = "";
        if (defBlock.contains("next") && defBlock["next"].is_string()) {
            firstStatement = defBlock["next"].get<std::string>();
        }

        BytecodeChunk chunk;
        if (!firstStatement.empty()) {
            currentProccode = proccode;
            compileSequence(firstStatement, chunk);
            currentProccode = "";
        }
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::RETURN));

        size_t baseOffset = targetDef.bytecode.size();
        for (const auto &patch : chunk.unresolvedProcedureCalls) {
            allPatches.push_back({patch.proccode, baseOffset + patch.codeOffset});
        }

        targetDef.bytecode.insert(targetDef.bytecode.end(), chunk.code.begin(), chunk.code.end());
        info.compiled = true;
    }

    const auto &hatMap = HatBlockRegistry::getHatMap();

    for (const auto &[id, data] : blocksJson.items()) {
        if (!data.is_object()) continue;
        if (!data.contains("topLevel") || !data["topLevel"].get<bool>()) continue;
        if (!data.contains("opcode")) continue;

        std::string opcode = data["opcode"].get<std::string>();
        if (opcode == "procedures_definition") continue;

        auto it = hatMap.find(opcode);
        if (it == hatMap.end()) {
            continue;
        }

        HatParseResult hatRes = it->second(*this, data);
        if (!hatRes.isValid) continue;

        if (hatRes.hatType == HatType::THIS_SPRITE_CLICKED || hatRes.hatType == HatType::STAGE_CLICKED) {
            EntityManager::transforms[spriteIndex].setShouldClick(true);
        }

        uint32_t currentBytecodeOffset = static_cast<uint32_t>(targetDef.bytecode.size());

        targetDef.hatListeners.push_back(HatListener{
            static_cast<uint16_t>(hatRes.hatType),
            hatRes.eventParamId,
            currentBytecodeOffset});

        std::string firstStatement = "";
        if (data.contains("next") && data["next"].is_string()) {
            firstStatement = data["next"].get<std::string>();
        }

        BytecodeChunk chunk;
        if (!firstStatement.empty()) {
            compileSequence(firstStatement, chunk);
        }
        chunk.emitOpcode(static_cast<uint16_t>(Opcode::RETURN));

        size_t baseOffset = targetDef.bytecode.size();
        for (const auto &patch : chunk.unresolvedProcedureCalls) {
            allPatches.push_back({patch.proccode, baseOffset + patch.codeOffset});
        }

        targetDef.bytecode.insert(targetDef.bytecode.end(), chunk.code.begin(), chunk.code.end());
    }

    procedureTable.resolvePatches(targetDef.bytecode, allPatches);
}
void CompilerContext::parseVariables(const nlohmann::json &vars) {
    Variables &variables = EntityManager::variables[spriteIndex];
    std::unordered_map<std::string, uint16_t> &varMap = targetDef.variables;

    if (!vars.empty()) {
        for (const auto &[id, data] : vars.items()) {
            varMap[id] = variables.orderedKeys.size();
            variables.orderedKeys.push_back({});
            Variable &newVar = variables.orderedKeys.back();
            newVar.value = Value::fromJson(data[1]);

#ifdef ENABLE_CLOUDVARS
            newVar.cloud = data.size() == 3 && data[2].get<bool>();
            EngineState::cloudProject = EngineState::cloudProject || newVar.cloud;
#endif
        }
    }
}

void CompilerContext::parseLists(const nlohmann::json &listsJson) {
    Lists &lists = EntityManager::lists[spriteIndex];
    if (!listsJson.empty()) {
        for (const auto &[id, data] : listsJson.items()) {
            lists.orderedKeys.push_back({});
            ScratchList &newList = lists.orderedKeys.back();
            for (const auto &listItem : data[1]) {
                newList.items.push_back(Value::fromJson(listItem));
            }
            targetDef.lists[id] = lists.orderedKeys.size() - 1;
        }
    }
}

void CompilerContext::parseSounds(const nlohmann::json &soundsJson) {
    std::vector<Sound> &sounds = EntityManager::blueprints[spriteIndex].sounds;
    for (const auto &[id, data] : soundsJson.items()) {
        sounds.emplace_back();
        Sound &newSound = sounds.back();
        newSound.id = data["assetId"];
        newSound.name = data["name"];
        newSound.fullName = data["md5ext"];
        newSound.dataFormat = data["dataFormat"];
        newSound.sampleRate = data.value("rate", -1); // We don't actually use these values so -1 should be fine
        newSound.sampleCount = data.value("sampleCount", -1);
    }
}

void CompilerContext::parseCostumes(const nlohmann::json &costumesJson) {
    std::vector<Costume> &costumes = EntityManager::blueprints[spriteIndex].costumes;
    for (const auto &[id, data] : costumesJson.items()) {
        costumes.emplace_back();
        Costume &newCostume = costumes.back();
        newCostume.id = data["assetId"];
        if (data.contains("name")) {
            newCostume.name = data["name"];
        }
        if (data.contains("bitmapResolution")) {
            newCostume.bitmapResolution = data["bitmapResolution"];
        } else newCostume.bitmapResolution = 1;
        if (data.contains("dataFormat")) {
            newCostume.dataFormat = data["dataFormat"];
            newCostume.isSVG = (newCostume.dataFormat == "svg" || newCostume.dataFormat == "SVG");
        }
        if (data.contains("md5ext")) {
            newCostume.fullName = data["md5ext"];
        }
        if (data.contains("rotationCenterX")) {
            newCostume.rotationCenterX = data["rotationCenterX"];
            if (EngineState::bitmapHalfQuality && !newCostume.isSVG && newCostume.bitmapResolution == 2) newCostume.rotationCenterX /= 2;
        } else newCostume.rotationCenterX = -6767.6767; // will get changed once costume image is loaded
        if (data.contains("rotationCenterY")) {
            newCostume.rotationCenterY = data["rotationCenterY"];
            if (EngineState::bitmapHalfQuality && !newCostume.isSVG && newCostume.bitmapResolution == 2) newCostume.rotationCenterY /= 2;
        } else newCostume.rotationCenterY = -6767.6767; // will get changed once costume image is loaded
    }
}

void CompilerContext::parseBroadcasts(const nlohmann::json &broadcastsJson) {
    for (const auto &[id, data] : broadcastsJson.items()) {
        targetDef.broadcasts[data] = targetDef.broadcasts.size() + 1;
    }
}