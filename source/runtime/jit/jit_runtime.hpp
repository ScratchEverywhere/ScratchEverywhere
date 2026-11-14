#pragma once
#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "blockExecutor.hpp"
#include "bytecode_analysis.hpp"
#include "ir/lir_caps.hpp"
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
#include <string>
#include <unordered_map>

namespace BytecodeJitInternal::Runtime {

struct ListLayout {
    bool ok = false;
    BytecodeJitLir::Word items = 0;
    BytecodeJitLir::Word valueSize = 0;
    BytecodeJitLir::Word tag = 0, flags = 0, number = 0;
    BytecodeJitLir::Word numberTag = 0, stringTag = 0, booleanTag = 0;
    uint8_t typedFlags = 0;
};

inline constexpr uint32_t maxTemps = 64;
alignas(64) inline Value temps[maxTemps];
inline uint8_t tempLazy[maxTemps];
alignas(64) inline Value shadowValues[maxPromoted];
inline std::shared_ptr<const std::string> untouchedText[maxPromoted];
inline Value untouchedValue[maxPromoted];
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
inline BytecodeJitLir::Word stringChar[maxPromoted];

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

struct StringLayout {
    bool ok = false;
    BytecodeJitLir::Word pointer = 0, size = 0, capacity = 0, buffer = 0, smallCapacity = 0;
};
const StringLayout &stringLayout();
Value materializeVariableSlow(Context *ctx, uint32_t k);
const Value *untouchedTextValue(Context *ctx, uint32_t k);
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
    case ValueEntry::Boolean: {
        const double d = ctx->slots[ctx->code->slotValues + position];
        return Value(bitsOf(d) != 0);
    }
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

void LIR_FUNC jitPushColorConst(Context *ctx, BytecodeJitLir::Word index);
void LIR_FUNC jitPushColorVariable(Context *ctx, BytecodeJitLir::Word variable);
void LIR_FUNC jitPushColorPromoted(Context *ctx, BytecodeJitLir::Word k);
void LIR_FUNC jitCallColorVoid(Context *ctx, void (*fn)(Sprite *, Color));
BytecodeJitLir::Word LIR_FUNC jitCallValueBool(Context *ctx, const ValueCallDescriptor *call);
void LIR_FUNC jitConvertValueToColor(Context *ctx, const ValueCallDescriptor *call);
BytecodeJitLir::Word LIR_FUNC jitConvertValueToBoolean(Context *ctx, const ValueCallDescriptor *call);
double LIR_FUNC jitRandomValues(Context *ctx, const ValueCallDescriptor *call);
void LIR_FUNC jitCostumeNumberName(Context *ctx, BytecodeJitLir::Word position, BytecodeJitLir::Word byNumber);

void refreshDepthLimit(Context *ctx);
void refreshSave(Context *ctx);
void loadFrame(Context *ctx, const EntryDescriptor &entry);
BytecodeJitLir::Word LIR_FUNC jitEnter(Context *ctx, BytecodeJitLir::Word entryId);
void syncStacks(Context *ctx, const ExitDescriptor *state);
void commitVariables(Context *ctx);
void materializeLazyFrames(Context *ctx);
BytecodeJitLir::Word LIR_FUNC jitExit(Context *ctx, const ExitDescriptor *exit);
BytecodeJitLir::Word LIR_FUNC jitLeave(Context *ctx, BytecodeJitLir::Word result);
int numberKindOf(const Value &value);
int variableNumberKind(Context *ctx, uint32_t k);
int argumentNumberKind(Context *ctx, uint32_t index);
bool lazyCall(Context *ctx, const CallDescriptor *call);
BytecodeJitLir::Word LIR_FUNC jitCall(Context *ctx, const CallDescriptor *call);
BytecodeJitLir::Word lazyReturn(Context *ctx, const ReturnDescriptor *ret);
BytecodeJitLir::Word LIR_FUNC jitReturn(Context *ctx, const ReturnDescriptor *ret);
void LIR_FUNC jitCallValueVoid(Context *ctx, const ValueCallDescriptor *call);
double LIR_FUNC jitRepeatRemaining(double counter);
double LIR_FUNC jitRepeatRound(double counter);
BytecodeJitLir::Word LIR_FUNC jitCompareUnordered(double a, double b, BytecodeJitLir::Word kind);
double LIR_FUNC jitMathOp(double value, BytecodeJitLir::Word op);
double LIR_FUNC jitRandomInt(double a, double b);
BytecodeJitLir::Word LIR_FUNC jitPreempt(Context *ctx, BytecodeJitLir::Word target);

void LIR_FUNC jitListItem(Context *ctx, const ListOpDescriptor *op);
void LIR_FUNC jitListReplaceAny(Context *ctx, const ListOpDescriptor *op);
void LIR_FUNC jitListInsertAny(Context *ctx, const ListOpDescriptor *op);
void LIR_FUNC jitListDeleteAny(Context *ctx, const ListOpDescriptor *op);
void LIR_FUNC jitListItemAny(Context *ctx, const ListOpDescriptor *op);
void LIR_FUNC jitStoreShadow(Context *ctx, const ShadowDescriptor *shadow);
void LIR_FUNC jitSetNaN(Context *ctx, BytecodeJitLir::Word k);
void LIR_FUNC jitStoreBoolean(Context *ctx, BytecodeJitLir::Word k, BytecodeJitLir::Word value);
BytecodeJitLir::Word LIR_FUNC jitCompareValues(Context *ctx, const CompareDescriptor *compare);
void LIR_FUNC jitListReplace(Context *ctx, const ListOpDescriptor *op);
void LIR_FUNC jitListAdd(Context *ctx, const ListOpDescriptor *op);
void LIR_FUNC jitListInsert(Context *ctx, const ListOpDescriptor *op);
void LIR_FUNC jitListDelete(Context *ctx, const ListOpDescriptor *op);
void LIR_FUNC jitListClear(Context *ctx, const ListOpDescriptor *op);
double LIR_FUNC jitListLength(Context *ctx, const ListOpDescriptor *op);
double LIR_FUNC jitListIndexOf(Context *ctx, const ListOpDescriptor *op);
BytecodeJitLir::Word LIR_FUNC jitListContains(Context *ctx, const ListOpDescriptor *op);

void refreshStringChar(uint32_t k);
bool stringVariableIsAscii(uint32_t k);
const std::string &stringOfVariable(Context *ctx, uint32_t k, uint32_t position);
const std::string &stringOf(Context *ctx, const StrEntry &entry, uint32_t position);
std::string readVariableString(Context *ctx, uint32_t k);
void setString(Context *ctx, uint32_t position, std::string &&value);
void setStringVariable(Context *ctx, uint32_t k, std::string &&value);
void LIR_FUNC jitEnsureStringNumber(Context *ctx, BytecodeJitLir::Word k);
void LIR_FUNC jitStringCompare(Context *ctx, const StringOp *op);
void LIR_FUNC jitStringAppend(Context *ctx, const StringOp *op);
void LIR_FUNC jitLetterOfVariable(Context *ctx, const StringOp *op);
void LIR_FUNC jitStringOp(Context *ctx, const StringOp *op);
} // namespace BytecodeJitInternal::Runtime

#endif
