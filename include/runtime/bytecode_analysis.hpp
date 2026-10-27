#pragma once
#include "bytecode.hpp"
#include <cstdint>
#include <se_export.hpp>
#include <string>
#include <vector>

enum class BytecodeValueType : uint8_t {
    Unreached,
    Number,
    String,
    Boolean,
    Color,
    Unknown,
};

struct SE_EXPORT BytecodeAnalysisPolicy {
    bool warpYieldForgets = true;

    bool positionalCallsForget = false;

    bool genericNativeCallsForget = true;
};

struct SE_EXPORT BytecodeAbstractState {
    std::vector<BytecodeValueType> values;
    uint32_t numbers = 0;
    uint32_t strings = 0;
    uint32_t booleans = 0;
    uint32_t colors = 0;
    std::vector<BytecodeValueType> variables;
};

struct SE_EXPORT BytecodeEntryResult {
    BytecodeIndex entryInstruction = invalidBytecodeIndex;
    bool isProcedure = false;
    BytecodeIndex index = invalidBytecodeIndex;
    bool warp = false;
    bool ok = true;
    std::string error;
    uint32_t reachableInstructions = 0;
};

struct SE_EXPORT BytecodeAnalysisStats {
    uint32_t instructions = 0;
    uint32_t reachable = 0;
    uint32_t blocks = 0;

    uint32_t pushVarValue = 0;
    uint32_t pushVarValueKnown = 0;
    uint32_t storeVarValue = 0;
    uint32_t storeVarValueKnown = 0;
    uint32_t convertValueToTyped = 0;
    uint32_t convertValueToTypedKnown = 0;
    uint32_t convertValueToTypedIdentity = 0;
    uint32_t pushArg = 0;

    uint32_t demotedAccesses = 0;
    uint32_t demotedAccessesKnown = 0;
};

class SE_EXPORT BytecodeAnalysis {
  public:
    BytecodeAnalysis(const BytecodeChunk &chunk, const BytecodeLinkTable &link, BytecodeAnalysisPolicy policy = {});

    bool run();

    const std::vector<BytecodeEntryResult> &entries() const { return entries_; }
    const BytecodeAnalysisStats &stats() const { return stats_; }

    std::string report() const;

    BytecodeValueType variableTypeBefore(BytecodeIndex pc, BytecodeIndex variableSlot) const;

  private:
    struct Block {
        BytecodeIndex start = 0;
        BytecodeIndex end = 0;
        bool reached = false;
        bool warp = false;
        int entry = -1;
        BytecodeAbstractState in;
    };

    const BytecodeChunk &chunk_;
    const BytecodeLinkTable &link_;
    BytecodeAnalysisPolicy policy_;

    std::vector<Block> blocks_;
    std::vector<uint32_t> blockOfInstruction_;
    std::vector<BytecodeEntryResult> entries_;
    BytecodeAnalysisStats stats_;
    std::vector<BytecodeValueType> declaredVariableTypes_;

    void buildBlocks();
    void declareVariables();
    bool step(BytecodeAbstractState &state, const BytecodeInstruction &instr, bool warp, std::string &error) const;
    void forgetVariables(BytecodeAbstractState &state) const;
    bool merge(Block &target, const BytecodeAbstractState &incoming, int entry, std::string &error);
    void countStats();
};
