#pragma once
#include "color.hpp"
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

enum class BytecodeStackKind : uint8_t {
    Number,
    String,
    Boolean,
    Color,
    Value,
};

struct SE_EXPORT BytecodeInputSignature {
    std::string name;
    BytecodeStackKind kind;
};

enum class BytecodeOp : uint8_t {
    Nop,

    PushConstNumber,
    PushConstString,
    PushConstBoolean,
    PushConstColor,
    PushConstValue,

    PushVarNumber,
    PushVarString,
    PushVarBoolean,
    PushVarColor,
    PushVarValue,
    PushList,
    PushArg,

    StoreVarNumber,
    StoreVarString,
    StoreVarBoolean,
    StoreVarColor,
    StoreVarValue,

    CallNativeNumber,
    CallNativeString,
    CallNativeBoolean,
    CallNativeColor,
    CallNativeValue,

    CallProcedure,
    Return,

    CallLegacyReporter,

    EscapeToLegacyThread,

    ConvertNumberToValue,
    ConvertStringToValue,
    ConvertBooleanToValue,
    ConvertColorToValue,

    ConvertValueToNumber,
    ConvertValueToString,
    ConvertValueToBoolean,
    ConvertValueToColor,

    Jump,
    JumpIfFalse,
    JumpIfTrue,

    PopNumber,
    PopString,
    PopBoolean,
    PopColor,
    PopValue,

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

    BytecodeIndex argumentNamesStart = invalidBytecodeIndex;
    BytecodeIndex argumentNamesCount = 0;
};

struct SE_EXPORT BytecodeScript {
    BytecodeIndex opcodeIndex = invalidBytecodeIndex;
    BytecodeIndex entryInstruction = invalidBytecodeIndex;
};

struct SE_EXPORT BytecodeChunk {
    std::vector<BytecodeInstruction> instructions;

    std::vector<double> numberConstants;
    std::vector<std::string> stringConstants;
    std::vector<bool> boolConstants;
    std::vector<Color> colorConstants;
    std::vector<Value> constants;

    std::vector<std::string> strings;

    std::vector<BytecodeIndex> opcodeSlots;
    std::vector<BytecodeIndex> variableSlots;
    std::vector<BytecodeIndex> listSlots;

    std::vector<BytecodeStackKind> paramKinds;
    std::vector<BytecodeIndex> paramKindsStart;
    std::vector<BytecodeIndex> paramKindsCount;

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

using BytecodePositionalFnNN_N = double (*)(double, double);

struct SE_EXPORT BytecodePositionalFn {
    BytecodePositionalFnNN_N nn_n = nullptr;
};

struct BlockFunc;
struct Variable;
struct List;
struct Block;
struct ScriptThread;

struct SE_EXPORT BytecodeLinkTable {
    std::vector<const BlockFunc *> opcodes;
    std::vector<BytecodePositionalFn> positional;
    std::vector<Variable *> variables;
    std::vector<List *> lists;
    std::vector<Block *> escapeBlocks;
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

    std::vector<double> numberStack;
    std::vector<std::string> stringStack;
    std::vector<bool> boolStack;
    std::vector<Color> colorStack;
    std::vector<Value> valueStack;

    std::vector<BytecodeCallFrame> callStack;
    std::vector<uint32_t> activeProcedureCalls;

    std::unordered_map<BytecodeIndex, BytecodeFrame> frames;
    Value returnValue;

    ScriptThread *legacyThread = nullptr;
    bool escapedToLegacy = false;

    bool isRecursiveProcedureCall(BytecodeIndex procedureIndex) const {
        return procedureIndex < activeProcedureCalls.size() && activeProcedureCalls[procedureIndex] > 0;
    }
};
