#pragma once
#include "bytecode.hpp"
#include <cstdint>
#include <se_export.hpp>
#include <string>
#include <unordered_map>
#include <vector>

enum class BytecodeValueType : uint8_t {
    Unreached,
    Number,
    String,
    Boolean,
    Color,
    NumericString,
    NumberLike,
    Unknown,
};

struct SE_EXPORT BytecodeAnalysisPolicy {
    bool warpYieldForgets = true;
    bool positionalCallsForget = false;
    bool genericNativeCallsForget = true;
    bool interprocedural = true;
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

    uint32_t pushArgKnown = 0;
    uint32_t procedureCalls = 0;
    uint32_t procedureCallsKeepingFacts = 0;
    uint32_t procedureValueCalls = 0;
    uint32_t procedureValueCallsTyped = 0;
};

class SE_EXPORT BytecodeAnalysis {
  public:
    BytecodeAnalysis(const BytecodeChunk &chunk, const BytecodeLinkTable &link, BytecodeAnalysisPolicy policy = {}, std::vector<BytecodeValueType> variableSeed = {});

    const std::vector<uint8_t> &writtenTypes() const { return written_; }
    const std::vector<BytecodeValueType> &declaredTypes() const { return declared_; }

    const std::vector<BytecodeValueType> &readFacts() const { return readFacts_; }

    static bool isCanonicalNumericString(const std::string &text);

    static uint8_t typeBit(BytecodeValueType type);
    static BytecodeValueType collapse(uint8_t typeBits);

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
        int procedure = -1;
        BytecodeAbstractState in;
    };

    const BytecodeChunk &chunk_;
    const BytecodeLinkTable &link_;
    BytecodeAnalysisPolicy policy_;

    std::vector<Block> blocks_;
    std::vector<uint32_t> blockOfInstruction_;
    std::vector<BytecodeEntryResult> entries_;
    BytecodeAnalysisStats stats_;
    std::vector<BytecodeValueType> declared_;
    std::vector<BytecodeValueType> seed_;

    std::vector<std::vector<BytecodeValueType>> params_;
    std::vector<BytecodeValueType> returns_;
    std::vector<bool> procedureSuspends_;
    std::vector<std::vector<bool>> procedureWrites_;
    std::vector<std::vector<bool>> procedureReaches_;

    mutable std::vector<std::vector<BytecodeValueType>> nextParams_;
    mutable std::vector<BytecodeValueType> nextReturns_;
    mutable std::vector<bool> callSeen_;
    mutable std::vector<uint8_t> written_;
    std::vector<BytecodeValueType> readFacts_;

    void buildBlocks();
    void declareVariables();
    void computeProcedureEffects();
    void runDataflow();
    bool step(BytecodeAbstractState &state, const BytecodeInstruction &instr, bool warp, int procedure, std::string &error) const;
    void forgetVariables(BytecodeAbstractState &state) const;
    bool merge(Block &target, const BytecodeAbstractState &incoming, int entry, std::string &error);
    void countStats();
    void recordWrite(BytecodeIndex slot, BytecodeValueType type) const;
};

#include <memory>
#include <string_view>
#include <unordered_set>

class SE_EXPORT BytecodeProgramAnalysis {
  public:
    struct Input {
        std::string name;
        const BytecodeChunk *chunk = nullptr;
        const BytecodeLinkTable *link = nullptr;
    };

    struct Environment {
        bool customExtensions = false;
        std::unordered_set<std::string> sliderVariables;
    };

    BytecodeProgramAnalysis(std::vector<Input> inputs, Environment environment, BytecodeAnalysisPolicy policy = {});

    bool run();

    const std::vector<Input> &inputs() const { return inputs_; }
    const std::vector<std::unique_ptr<BytecodeAnalysis>> &chunks() const { return analyses_; }
    BytecodeAnalysisStats totalStats() const;
    int rounds() const { return rounds_; }

    std::string variableReport() const;

  private:
    std::vector<Input> inputs_;
    Environment environment_;
    BytecodeAnalysisPolicy policy_;
    std::vector<std::unique_ptr<BytecodeAnalysis>> analyses_;
    std::unordered_map<std::string, uint8_t> variableTypes_;
    int rounds_ = 0;
};
