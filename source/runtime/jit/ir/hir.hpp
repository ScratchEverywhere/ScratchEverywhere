#pragma once
#include "bytecode.hpp"
#include "bytecode_analysis.hpp"

#ifdef SE_JIT

#include <cstdint>
#include <string>
#include <vector>

namespace BytecodeJitIR {

enum class Type : uint8_t { None,
                            F64,
                            Bool,
                            Str,
                            Color,
                            Val };

inline Type typeOf(BytecodeStackKind kind) {
    switch (kind) {
    case BytecodeStackKind::Number:
        return Type::F64;
    case BytecodeStackKind::String:
        return Type::Str;
    case BytecodeStackKind::Boolean:
        return Type::Bool;
    case BytecodeStackKind::Color:
        return Type::Color;
    default:
        return Type::Val;
    }
}

const char *typeName(Type type);

using ValueId = uint32_t;
using BlockId = uint32_t;
inline constexpr ValueId noValue = 0xFFFFFFFFu;
inline constexpr BlockId noBlock = 0xFFFFFFFFu;

struct ValueInfo {
    Type type = Type::None;
    BytecodeValueType kind = BytecodeValueType::Unknown;
    BlockId block = noBlock;
    uint32_t index = 0;
    bool isParameter = false;
};

enum class Op : uint8_t {
    Bytecode,
    Const,
    NumCompare,
    RepeatStep,
    ForEachStep,
};

enum class Terminator : uint8_t { None,
                                  Br,
                                  CondBr,
                                  Ret,
                                  Halt,
                                  Escape,
};

struct Instr {
    Op op = Op::Bytecode;
    BytecodeOp bc = BytecodeOp::Nop;
    uint32_t a = 0, b = 0;
    BytecodeIndex pc = 0;
    std::vector<ValueId> args;
    std::vector<ValueId> results;
};

struct Edge {
    BlockId target = noBlock;
    std::vector<ValueId> args;
};

struct Block {
    BytecodeIndex startPc = 0, endPc = 0;
    std::vector<ValueId> params;
    std::vector<Instr> instrs;
    Terminator term = Terminator::None;
    ValueId cond = noValue;
    Edge edges[2];
    bool reachable = false;
};

struct EntryPoint {
    BlockId block = noBlock;
    BytecodeIndex pc = 0;
    bool isProcedure = false;
    uint32_t index = 0;
};

struct Function {
    std::vector<Block> blocks;
    std::vector<ValueInfo> values;
    std::vector<EntryPoint> entries;

    ValueId newValue(Type type, BytecodeValueType kind = BytecodeValueType::Unknown) {
        ValueInfo info;
        info.type = type;
        info.kind = kind;
        values.push_back(info);
        return static_cast<ValueId>(values.size() - 1);
    }
};

struct BuildStats {
    uint32_t instructions = 0;
    uint32_t blocks = 0;
    uint32_t structured = 0;
    uint32_t generic = 0;
    std::vector<uint32_t> byBytecode;
};

struct BuildResult {
    Function function;
    BuildStats stats;
    std::vector<std::string> errors;
};

BuildResult build(const BytecodeChunk &chunk, const BytecodeLinkTable &link, const BytecodeAnalysis &analysis);

std::vector<std::string> verify(const Function &function);

std::string print(const Function &function, const BytecodeChunk *chunk = nullptr);
std::string summary(const BuildResult &result, const std::vector<std::string> &verifyErrors);

} // namespace BytecodeJitIR

#endif
