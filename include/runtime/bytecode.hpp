#pragma once
#include "value.hpp"
#include <cstdint>
#include <limits>
#include <se_export.hpp>
#include <string>
#include <timer.hpp>
#include <unordered_map>
#include <vector>

// used instead of pointers to make this serializable (yay)
using BytecodeIndex = uint32_t;
inline constexpr BytecodeIndex invalidBytecodeIndex = std::numeric_limits<BytecodeIndex>::max();

enum class BytecodeOp : uint8_t {
    Nop, // idek if we need this

    PushConst,
    PushVar,
    PushList,
    PushArg,

    StoreVar,

    CallNative,
    CallProcedure,
    Return,

    Jump,
    JumpIfFalse,
    JumpIfTrue,
    Pop,

    Yield,
    Halt,
};

struct SE_EXPORT BytecodeInstruction {
    BytecodeOp op = BytecodeOp::Nop;
    BytecodeIndex a = invalidBytecodeIndex;
    BytecodeIndex b = invalidBytecodeIndex;
};

struct SE_EXPORT BytecodeProcedure {
    BytecodeIndex procCodeIndex = invalidBytecodeIndex;
    BytecodeIndex entryInstruction = invalidBytecodeIndex;
    BytecodeIndex argCount = 0;
    bool runWithoutScreenRefresh = false;
};

struct SE_EXPORT BytecodeScript {
    BytecodeIndex opcodeIndex = invalidBytecodeIndex;
    BytecodeIndex entryInstruction = invalidBytecodeIndex;
};

struct SE_EXPORT BytecodeChunk {
    std::vector<BytecodeInstruction> instructions;
    std::vector<Value> constants;
    std::vector<std::string> strings;

    std::vector<BytecodeIndex> opcodeSlots;
    std::vector<BytecodeIndex> variableSlots;
    std::vector<BytecodeIndex> listSlots;

    std::vector<BytecodeProcedure> procedures;
    std::vector<BytecodeScript> scripts;

    BytecodeIndex internString(const std::string &s) {
        for (BytecodeIndex i = 0; i < strings.size(); ++i) {
            if (strings[i] == s) return i;
        }
        strings.push_back(s);
        return static_cast<BytecodeIndex>(strings.size() - 1);
    }
};

struct BlockFunc;
struct Variable;
struct List;

struct SE_EXPORT BytecodeLinkTable {
    std::vector<const BlockFunc *> opcodes;
    std::vector<Variable *> variables;
    std::vector<List *> lists;
};

// runtime stuff:

class Sprite;

struct SE_EXPORT BytecodeFrame {
    int completedSteps = 0;
    double repeatTimes = -1;
    double waitDuration = 0;
    double glideStartX = 0, glideStartY = 0;
    double glideEndX = 0, glideEndY = 0;
    int musicChannel = 0;
    std::string name;

    Timer waitTimer;
    std::vector<uint64_t> threads;

    void clear() {
        completedSteps = 0;
        repeatTimes = -1;
        waitDuration = 0;
        glideStartX = glideStartY = glideEndX = glideEndY = 0;
        waitTimer = Timer();
        name = "";
        threads.clear();
    }
};

struct SE_EXPORT BytecodeCallFrame {
    BytecodeIndex procedureIndex = invalidBytecodeIndex;
    BytecodeIndex returnInstruction = invalidBytecodeIndex;
    std::vector<Value> args;
};

struct SE_EXPORT BytecodeThread {
    uint64_t id = 0;
    Sprite *sprite = nullptr;
    const BytecodeChunk *chunk = nullptr;
    const BytecodeLinkTable *link = nullptr;
    BytecodeThread *parentThread = nullptr;

    BytecodeIndex pc = invalidBytecodeIndex;
    BytecodeIndex hatInstruction = invalidBytecodeIndex;

    int finished = true;
    bool withoutScreenRefresh = false;

    std::vector<Value> valueStack;
    std::vector<BytecodeCallFrame> callStack;

    std::vector<uint32_t> activeProcedureCalls;

    std::unordered_map<BytecodeIndex, BytecodeFrame> frames;
    Value returnValue;

    bool isRecursiveProcedureCall(BytecodeIndex procedureIndex) const {
        return procedureIndex < activeProcedureCalls.size() && activeProcedureCalls[procedureIndex] > 0;
    }
};
