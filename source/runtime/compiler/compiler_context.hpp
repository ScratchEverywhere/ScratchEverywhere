#pragma once
#include <string>

#include "../data/blueprint.hpp"
#include "bytecode_chunk.hpp"
#include <nlohmann/json.hpp>

struct CompileResult {
    bool isConstant = false;
    Value constantValue = Value();
    BytecodeChunk chunk;

    static CompileResult Constant(Value val) {
        return {true, val, {}};
    }
    static CompileResult Dynamic(BytecodeChunk chunk = {}) {
        return {false, Value(), std::move(chunk)};
    }
};

struct ProcedureCompileInfo {
    uint16_t slot;
    bool compiled = false;
    bool warp = false;
    std::vector<std::string> argumentIdsInOrder;
};

class ProcedureTable {
  public:
    std::unordered_map<std::string, ProcedureCompileInfo> proccodeToInfo;
    std::unordered_map<std::string, uint32_t> proccodeToAddress;
    std::unordered_map<std::string, std::string> blockIdToProccode;
    std::vector<std::string> worklist;

    uint16_t getOrCreateSlot(const std::string &proccode) {
        auto it = proccodeToInfo.find(proccode);
        if (it != proccodeToInfo.end()) return it->second.slot;
        uint16_t slot = static_cast<uint16_t>(proccodeToInfo.size());
        proccodeToInfo[proccode] = {slot};
        worklist.push_back(proccode);
        return slot;
    }

    uint16_t getSlot(const std::string &proccode) {
        if (proccodeToInfo.count(proccode) > 0) return proccodeToInfo[proccode].slot;
        return -1;
    }

    void addBlockName(std::string proccode, std::string blockId) {
        if (blockIdToProccode.count(blockId) > 0) return;
        getOrCreateSlot(proccode);
        blockIdToProccode[blockId] = proccode;
    }

    void registerAddress(const std::string &proccode, uint32_t address) {
        proccodeToAddress[proccode] = address;
    }

    void resolvePatches(std::vector<uint16_t> &finalBytecode, const std::vector<BytecodeChunk::ProcedureCallPatch> &patches) {
        for (const auto &patch : patches) {
            auto it = proccodeToAddress.find(patch.proccode);
            if (it != proccodeToAddress.end()) {
                uint32_t targetAddress = it->second;
                finalBytecode[patch.codeOffset] = static_cast<uint16_t>(targetAddress & 0xFFFF);
                finalBytecode[patch.codeOffset + 1] = static_cast<uint16_t>(targetAddress >> 16);
            }
        }
    }
};

class CompilerContext {
  public:
    const nlohmann::json &blocksJson;
    std::string currentBlock;
    std::string currentProccode;
    TargetDefinition &targetDef;
    uint32_t spriteIndex;
    ProcedureTable procedureTable;

    const ProcedureCompileInfo *getCurrentProcedureInfo() const {
        if (currentProccode.empty()) return nullptr;
        auto it = procedureTable.proccodeToInfo.find(currentProccode);
        if (it == procedureTable.proccodeToInfo.end()) return nullptr;
        return &it->second;
    }

    CompileResult compileExpression(const std::string blockId);
    void compileSequence(std::string firstStatement, BytecodeChunk &out);
    std::string resolveFieldValue(const std::string &name) const;
    void parseProcedures();

    CompilerContext(const nlohmann::json &json, TargetDefinition &target, uint32_t spriteIndex)
        : blocksJson(json), targetDef(target), spriteIndex(spriteIndex) {}

    CompileResult compileInput(const std::string &inputName);

    const nlohmann::json &getBlock(const std::string &blockId) const { return blocksJson[blockId]; }
    const nlohmann::json &getCurrentBlock() const { return blocksJson[currentBlock]; }
    void parseScripts();
    void parseVariables(const nlohmann::json &vars);
    void parseLists(const nlohmann::json &lists);
    void parseSounds(const nlohmann::json &sounds);
    void parseCostumes(const nlohmann::json &costumes);
    void parseBroadcasts(const nlohmann::json &broadcastsJson);
};

static inline CompilerContext *stageContext = nullptr;