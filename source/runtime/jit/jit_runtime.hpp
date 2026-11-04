#pragma once
#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "blockExecutor.hpp"
#include "bytecode_analysis.hpp"
#include "jit_types.hpp"
#include "renderer.hpp"
#include "runtime.hpp"
#include "runtime/data/blueprint.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <log.hpp>
#include <map>
#include <memory>
#include <set>
#include <sljitLir.h>
#include <string>
#include <unordered_map>

namespace BytecodeJitInternal::Runtime {

struct ListLayout {
    bool ok = false;
    sljit_sw items = 0;
    sljit_sw valueSize = 0;
    sljit_sw tag = 0, flags = 0, number = 0;
    sljit_sw numberTag = 0, stringTag = 0;
    uint8_t typedFlags = 0;
};

inline constexpr uint32_t maxTemps = 64;
alignas(64) inline Value temps[maxTemps];
inline uint8_t tempLazy[maxTemps];
alignas(64) inline Value shadowValues[maxPromoted];
inline std::vector<double> saveStack;
struct NativeFrame {
    const CallDescriptor *call;
    uint64_t numberAndBooleanBase;
    uint64_t stringAndValueBase;
    uint64_t flags;
};
inline constexpr uint64_t lazyFlag = 1, previousWarpFlag = 2;
inline constexpr uint32_t maxTypedArguments = 30;
[[gnu::noinline]] Value typedNumberValue(double number);
inline uint64_t frameTyped(const NativeFrame &frame) { return (frame.flags >> 2) & 0x3FFFFFFF; }
inline NativeFrame frames[MAX_CALL_DEPTH + 1];
inline constexpr uint32_t maxStrs = 64;
inline std::string strs[maxStrs];
inline std::string stringVariables[maxPromoted];
inline int8_t stringAscii[maxPromoted];
inline sljit_sw stringChar[maxPromoted];

inline int64_t bitsOf(double d) {
    int64_t bits;
    std::memcpy(&bits, &d, sizeof bits);
    return bits;
}

inline Variable *variableAt(const BytecodeThread &thread, BytecodeIndex slot) {
    if (thread.link == nullptr || slot >= thread.link->variables.size()) return nullptr;
    return thread.link->variables[slot];
}

inline List *listAt(const BytecodeThread &thread, uint32_t slot) {
    if (thread.link == nullptr || slot >= thread.link->lists.size()) return nullptr;
    return thread.link->lists[slot];
}

inline size_t listPosition(double index, size_t size) {
    if (std::isnan(index)) index = 0.0;
    if (!std::isfinite(index)) return size;
    const double position = std::floor(index) - 1;
    if (position < 0 || position >= static_cast<double>(size)) return size;
    return static_cast<size_t>(position);
}

inline double fromBits(int64_t bits) {
    double d;
    std::memcpy(&d, &bits, sizeof d);
    return d;
}

Variable *resolveOrCreateVariable(BytecodeThread &thread, BytecodeIndex slot);
double rawVariableNumber(const Variable *var);
const ListLayout &listLayout();
Value materializeVariableSlow(Context *ctx, uint32_t k);
Value materializeEntrySlow(Context *ctx, const ValueEntry &entry, uint32_t position);

inline Value materializeVariable(Context *ctx, uint32_t k) {
    const int64_t kind = ctx->dirty[k];
    if (kind == 1 || kind == 2) return Value(ctx->slots[ctx->code->slotVariables + k]);
    return materializeVariableSlow(ctx, k);
}

inline Value materializeEntry(Context *ctx, const ValueEntry &entry, uint32_t position) {
    switch (entry.kind) {
    case ValueEntry::Number:
        return Value(ctx->slots[ctx->code->slotValues + position]);
    case ValueEntry::Constant:
        return ctx->chunk->constants[entry.constant];
    case ValueEntry::VarValue:
        return materializeVariable(ctx, entry.constant);
    case ValueEntry::Temp:
        if (tempLazy[position] == 1) return Value(ctx->slots[ctx->code->slotValues + position]);
        return temps[position];
    default:
        return materializeEntrySlow(ctx, entry, position);
    }
}

inline const Value &peekEntry(Context *ctx, const ValueEntry &entry, uint32_t position, Value &scratch) {
    switch (entry.kind) {
    case ValueEntry::Constant:
        return ctx->chunk->constants[entry.constant];
    case ValueEntry::Temp:
        if (tempLazy[position] != 1) return temps[position];
        break;
    default:
        break;
    }
    scratch = materializeEntry(ctx, entry, position);
    return scratch;
}

void refreshDepthLimit(Context *ctx);
void refreshSave(Context *ctx);
void loadFrame(Context *ctx, const EntryDescriptor &entry);
sljit_sw SLJIT_FUNC jitEnter(Context *ctx, sljit_sw entryId);
void syncStacks(Context *ctx, const ExitDescriptor *state);
void commitVariables(Context *ctx);
void materializeLazyFrames(Context *ctx);
sljit_sw SLJIT_FUNC jitExit(Context *ctx, const ExitDescriptor *exit);
sljit_sw SLJIT_FUNC jitLeave(Context *ctx, sljit_sw result);
int numberKindOf(const Value &value);
int variableNumberKind(Context *ctx, uint32_t k);
int argumentNumberKind(Context *ctx, uint32_t index);
bool lazyCall(Context *ctx, const CallDescriptor *call);
sljit_sw SLJIT_FUNC jitCall(Context *ctx, const CallDescriptor *call);
sljit_sw lazyReturn(Context *ctx, const ReturnDescriptor *ret);
sljit_sw SLJIT_FUNC jitReturn(Context *ctx, const ReturnDescriptor *ret);
void SLJIT_FUNC jitCallValueVoid(Context *ctx, const ValueCallDescriptor *call);
double SLJIT_FUNC jitRepeatRemaining(double counter);
sljit_sw SLJIT_FUNC jitCompareUnordered(double a, double b, sljit_sw kind);
double SLJIT_FUNC jitMathOp(double value, sljit_sw op);
sljit_sw SLJIT_FUNC jitPreempt(Context *ctx);

void SLJIT_FUNC jitListItem(Context *ctx, const ListOpDescriptor *op);
void SLJIT_FUNC jitListReplaceAny(Context *ctx, const ListOpDescriptor *op);
void SLJIT_FUNC jitListInsertAny(Context *ctx, const ListOpDescriptor *op);
void SLJIT_FUNC jitListDeleteAny(Context *ctx, const ListOpDescriptor *op);
void SLJIT_FUNC jitListItemAny(Context *ctx, const ListOpDescriptor *op);
void SLJIT_FUNC jitStoreShadow(Context *ctx, const ShadowDescriptor *shadow);
sljit_sw SLJIT_FUNC jitCompareValues(Context *ctx, const CompareDescriptor *compare);
void SLJIT_FUNC jitListReplace(Context *ctx, const ListOpDescriptor *op);
void SLJIT_FUNC jitListAdd(Context *ctx, const ListOpDescriptor *op);
void SLJIT_FUNC jitListInsert(Context *ctx, const ListOpDescriptor *op);
void SLJIT_FUNC jitListDelete(Context *ctx, const ListOpDescriptor *op);
void SLJIT_FUNC jitListClear(Context *ctx, const ListOpDescriptor *op);
double SLJIT_FUNC jitListLength(Context *ctx, const ListOpDescriptor *op);
double SLJIT_FUNC jitListIndexOf(Context *ctx, const ListOpDescriptor *op);
sljit_sw SLJIT_FUNC jitListContains(Context *ctx, const ListOpDescriptor *op);

void refreshStringChar(uint32_t k);
bool stringVariableIsAscii(uint32_t k);
const std::string &stringOfVariable(Context *ctx, uint32_t k, uint32_t position);
const std::string &stringOf(Context *ctx, const StrEntry &entry, uint32_t position);
std::string readVariableString(Context *ctx, uint32_t k);
void setString(Context *ctx, uint32_t position, std::string &&value);
void setStringVariable(Context *ctx, uint32_t k, std::string &&value);
void SLJIT_FUNC jitEnsureStringNumber(Context *ctx, sljit_sw k);
void SLJIT_FUNC jitStringCompare(Context *ctx, const StringOp *op);
void SLJIT_FUNC jitStringAppend(Context *ctx, const StringOp *op);
void SLJIT_FUNC jitLetterOfVariable(Context *ctx, const StringOp *op);
void SLJIT_FUNC jitStringOp(Context *ctx, const StringOp *op);
} // namespace BytecodeJitInternal::Runtime

#endif
