#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "blockExecutor.hpp"
#include "bytecode_analysis.hpp"
#include "renderer.hpp"
#include "runtime.hpp"
#include "runtime/data/blueprint.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <log.hpp>
#include <map>
#include <set>
#include <sljitLir.h>
#include <string>
#include <unordered_map>

namespace BytecodeJitInternal {

constexpr uint32_t kMaxSlots = 480;
constexpr uint32_t kMaxPromoted = 128;
constexpr uint32_t kPollInterval = 64;

struct StrEntry {
    enum Kind : uint8_t { Opaque,
                          Const,
                          Temp,
                          Variable };
    Kind kind = Opaque;
    uint32_t constant = 0;
};

struct EntryDescriptor {
    uint32_t numbers = 0;
    uint32_t booleans = 0;
    uint32_t strings = 0;
    std::vector<uint8_t> valueIsNumber;
    std::vector<uint32_t> loadVariables;
};

struct ValueEntry {
    enum Kind : uint8_t { Opaque,
                          Number,
                          Constant,
                          Like,
                          VarValue,
                          Argument,
                          Temp
    };
    Kind kind = Opaque;
    uint32_t constant = 0;
};

struct StringOp {
    enum Kind : uint8_t { PushVariable,
                          StoreVariable,
                          AppendVariable,
                          Compare,
                          CallStringString,
                          CallStringStringBool,
                          CallStringNumber,
                          CallNumberStringString,
                          CallSpriteStringNumber,
                          CallSpriteStringBool,
                          CallSpriteString,
                          VoidString,
                          VoidStringNumber,
                          LetterOfVariable,
                          LengthOfVariable,
                          LetterOf,
                          LengthOf,
                          ValueToString,
                          StringToValue };
    Kind kind = PushVariable;
    BytecodeCompareKind compare = BytecodeCompareKind::Equals;
    void *fn = nullptr;
    uint32_t variable = 0;
    StrEntry a, b;
    uint32_t aPosition = 0, bPosition = 0;
    ValueEntry value;
    uint32_t valuePosition = 0;
    uint32_t outPosition = 0;
    uint32_t numberIn = 0;
    uint32_t numberOut = 0;
};

struct ShadowDescriptor {
    uint32_t variable = 0;
    ValueEntry entry;
    uint32_t position = 0;
};

struct CompareDescriptor {
    BytecodeCompareKind kind = BytecodeCompareKind::Equals;
    ValueEntry a, b;
    uint32_t aPosition = 0, bPosition = 0;
};

struct ValueCallDescriptor {
    void (*fn)(Sprite *, const Value &) = nullptr;
    ValueEntry item;
    uint32_t itemPosition = 0;
};

struct ListOpDescriptor {
    uint32_t list = 0;
    uint32_t indexPosition = 0;
    ValueEntry index;
    ValueEntry item;
    uint32_t itemPosition = 0;
};

struct ExitDescriptor {
    uint32_t pc = 0;
    uint32_t result = 0;
    uint32_t numbers = 0;
    uint32_t booleans = 0;
    std::vector<ValueEntry> values;
    std::vector<StrEntry> strings;
};

struct CallDescriptor {
    ExitDescriptor *state = nullptr;
    uint32_t callPc = 0;
    uint32_t procedure = 0;
    uint32_t argCount = 0;
    bool wantsValue = false;
    uint32_t calleeEntry = 0;
    bool leanKinds = false;
    mutable int8_t leanCallee = -1;
};

struct ReturnDescriptor {
    ExitDescriptor *state = nullptr;
    uint32_t pc = 0;
    bool isReturn = false;
};

struct Code;

struct alignas(64) Context {
    BytecodeThread *thread = nullptr;
    const BytecodeChunk *chunk = nullptr;
    const Code *code = nullptr;
    void *sprite = nullptr;
    Timer *timer = nullptr;
    bool *timerStarted = nullptr;
    uint32_t numberBase = 0;
    uint32_t booleanBase = 0;
    uint32_t valueBase = 0;
    uint32_t nativeDepth = 0;
    uint32_t saveTop = 0;
    uint32_t tempHigh = 0;
    uint64_t shadowUsed[2] = {0, 0};
    uint64_t warp = 0;
    int64_t dirty[kMaxPromoted];
    double slots[kMaxSlots];
    uint32_t stringBase = 0;
    uint32_t strHigh = 0;
    uint64_t stringVariableUsed[2] = {0, 0};
};

struct Code {
    void *machine = nullptr;
    sljit_sw (*fn)(Context *, sljit_sw) = nullptr;

    std::vector<EntryDescriptor> entries;
    std::deque<ExitDescriptor> exits;
    std::deque<ListOpDescriptor> listOps;
    std::deque<ValueCallDescriptor> valueCalls;
    std::deque<ShadowDescriptor> shadows;
    std::deque<StringOp> stringOps;
    std::deque<double> doubles;
    std::deque<CompareDescriptor> compares;
    std::deque<CallDescriptor> calls;
    std::deque<ReturnDescriptor> returns;
    std::vector<int32_t> entryIdAt;
    std::vector<void *> entryTable;
    std::vector<uint32_t> promoted;
    uint32_t slotBooleans = 0;
    uint32_t slotValues = 0;
    uint32_t slotVariables = 0;

    ~Code() {
        if (machine != nullptr) sljit_free_code(machine, nullptr);
    }
};

} // namespace BytecodeJitInternal

struct BytecodeJitCode : BytecodeJitInternal::Code {};

namespace {

using namespace BytecodeJitInternal;

Variable *variableAt(const BytecodeThread &thread, BytecodeIndex slot) {
    if (thread.link == nullptr || slot >= thread.link->variables.size()) return nullptr;
    return thread.link->variables[slot];
}

Variable *resolveOrCreateVariable(BytecodeThread &thread, BytecodeIndex slot) {
    Variable *var = variableAt(thread, slot);
    if (var != nullptr) return var;
    if (thread.sprite == nullptr || thread.link == nullptr || thread.chunk == nullptr) return nullptr;
    if (slot >= thread.chunk->variableSlots.size() || thread.chunk->variableSlots[slot] >= thread.chunk->strings.size()) return nullptr;

    const std::string &id = thread.chunk->strings[thread.chunk->variableSlots[slot]];
    Variable &created = thread.sprite->variables[id];
    created.id = id;
    created.name = id;
    const_cast<BytecodeLinkTable *>(thread.link)->variables[slot] = &created;
    return &created;
}

double rawVariableNumber(const Variable *var) {
    if (var == nullptr) return 0.0;
    switch (var->value.index()) {
    case 1:
        return std::get<double>(var->value);
    case 2: {
        const auto &text = std::get<std::shared_ptr<const std::string>>(var->value);
        return text ? Conversions::stringToNumber(*text) : 0.0;
    }
    case 3:
        return std::get<bool>(var->value) ? 1.0 : 0.0;
    default:
        return std::get<Value>(var->value).numberRaw();
    }
}

Value materializeEntry(Context *ctx, const ValueEntry &entry, uint32_t position);
std::string readVariableString(Context *ctx, uint32_t k);

constexpr uint32_t kMaxTemps = 64;
alignas(64) Value gTemps[kMaxTemps];
alignas(64) Value gShadows[kMaxPromoted];
std::vector<double> gSave;
struct LeanFrame {
    uint64_t returnAndBase;
    uint64_t saved;
    uint64_t flags;
};
constexpr uint64_t kLeanFlag = uint64_t(1) << 34;
LeanFrame gLean[MAX_CALL_DEPTH + 1];
constexpr uint32_t kMaxStrs = 64;
std::string gStrs[kMaxStrs];
std::string gStringVariables[kMaxPromoted];
int8_t gStringAscii[kMaxPromoted];

bool stringVariableIsAscii(uint32_t k) {
    if (gStringAscii[k] < 0) {
        bool ascii = true;
        for (unsigned char ch : gStringVariables[k]) {
            if (ch >= 0x80) {
                ascii = false;
                break;
            }
        }
        gStringAscii[k] = ascii ? 1 : 0;
    }
    return gStringAscii[k] == 1;
}

Value materializeVariable(Context *ctx, uint32_t k) {
    const Code &code = *ctx->code;
    const int64_t kind = ctx->dirty[k];
    if (kind == 0) return BlockExecutor::getVariableValueAs<Value>(variableAt(*ctx->thread, code.promoted[k]));
    if (kind == 1 || kind == 2) return Value(ctx->slots[code.slotVariables + k]);
    if (kind == 3) return gShadows[k];
    if (kind == 4) return Value(gStringVariables[k]);
    return ctx->chunk->constants[kind - 5];
}

Value materializeEntry(Context *ctx, const ValueEntry &entry, uint32_t position) {
    switch (entry.kind) {
    case ValueEntry::Number:
        return Value(ctx->slots[ctx->code->slotValues + position]);
    case ValueEntry::Constant:
        return ctx->chunk->constants[entry.constant];
    case ValueEntry::VarValue:
        return materializeVariable(ctx, entry.constant);
    case ValueEntry::Argument: {
        const BytecodeThread &thread = *ctx->thread;
        if (thread.callStack.empty()) return Value();
        const BytecodeCallFrame &frame = thread.callStack.back();
        if (frame.stackBase < frame.argCount || entry.constant >= frame.argCount) return Value();
        const size_t index = static_cast<size_t>(frame.stackBase - frame.argCount) + entry.constant;
        return index < thread.valueStack.size() ? thread.valueStack[index] : Value();
    }
    case ValueEntry::Temp:
        return gTemps[position];
    default:
        return ctx->thread->valueStack[ctx->valueBase + position];
    }
}

List *listAt(const BytecodeThread &thread, uint32_t slot) {
    if (thread.link == nullptr || slot >= thread.link->lists.size()) return nullptr;
    return thread.link->lists[slot];
}

size_t listPosition(double index, size_t size) {
    if (std::isnan(index)) index = 0.0;
    if (!std::isfinite(index)) return size;
    const double position = std::floor(index) - 1;
    if (position < 0 || position >= static_cast<double>(size)) return size;
    return static_cast<size_t>(position);
}

int64_t bitsOf(double d) {
    int64_t bits;
    std::memcpy(&bits, &d, sizeof bits);
    return bits;
}

double fromBits(int64_t bits) {
    double d;
    std::memcpy(&d, &bits, sizeof d);
    return d;
}

void loadFrame(Context *ctx, const EntryDescriptor &entry) {
    BytecodeThread &thread = *ctx->thread;
    const Code &code = *ctx->code;

    if (thread.numberStack.size() < entry.numbers || thread.boolStack.size() < entry.booleans || thread.valueStack.size() < entry.valueIsNumber.size() || thread.stringStack.size() < entry.strings) {
        Log::logCritical("JIT: the thread's stacks are shallower than the analysis of the bytecode says", true);
        std::abort();
    }

    ctx->numberBase = static_cast<uint32_t>(thread.numberStack.size()) - entry.numbers;
    for (uint32_t i = 0; i < entry.numbers; i++)
        ctx->slots[i] = thread.numberStack[ctx->numberBase + i];

    ctx->booleanBase = static_cast<uint32_t>(thread.boolStack.size()) - entry.booleans;
    for (uint32_t i = 0; i < entry.booleans; i++)
        ctx->slots[code.slotBooleans + i] = fromBits(thread.boolStack[ctx->booleanBase + i] ? 1 : 0);

    ctx->stringBase = static_cast<uint32_t>(thread.stringStack.size()) - entry.strings;

    ctx->valueBase = static_cast<uint32_t>(thread.valueStack.size()) - static_cast<uint32_t>(entry.valueIsNumber.size());
    for (uint32_t i = 0; i < entry.valueIsNumber.size(); i++) {
        if (entry.valueIsNumber[i]) ctx->slots[code.slotValues + i] = thread.valueStack[ctx->valueBase + i].numberRaw();
    }

    ctx->warp = thread.withoutScreenRefresh ? 1 : 0;
}

void SLJIT_FUNC jitEnter(Context *ctx, sljit_sw entryId) {
    BytecodeThread &thread = *ctx->thread;
    const Code &code = *ctx->code;
    const EntryDescriptor &entry = code.entries[entryId];

    loadFrame(ctx, entry);

    std::memset(ctx->dirty, 0, code.promoted.size() * sizeof(int64_t));
    for (uint32_t k : entry.loadVariables) {
        ctx->slots[code.slotVariables + k] = rawVariableNumber(variableAt(thread, code.promoted[k]));
    }

    ctx->nativeDepth = 0;
    ctx->saveTop = 0;
}

void syncStacks(Context *ctx, const ExitDescriptor *state) {
    BytecodeThread &thread = *ctx->thread;
    const Code &code = *ctx->code;

    if (thread.numberStack.size() != ctx->numberBase + state->numbers) thread.numberStack.resize(ctx->numberBase + state->numbers);
    for (uint32_t i = 0; i < state->numbers; i++)
        thread.numberStack[ctx->numberBase + i] = ctx->slots[i];

    if (thread.boolStack.size() != ctx->booleanBase + state->booleans) thread.boolStack.resize(ctx->booleanBase + state->booleans);
    for (uint32_t i = 0; i < state->booleans; i++)
        thread.boolStack[ctx->booleanBase + i] = bitsOf(ctx->slots[code.slotBooleans + i]) != 0;

    if (thread.stringStack.size() != ctx->stringBase + state->strings.size()) thread.stringStack.resize(ctx->stringBase + state->strings.size());
    for (uint32_t i = 0; i < state->strings.size(); i++) {
        const StrEntry &entry = state->strings[i];
        if (entry.kind == StrEntry::Const) thread.stringStack[ctx->stringBase + i] = ctx->chunk->stringConstants[entry.constant];
        else if (entry.kind == StrEntry::Temp) thread.stringStack[ctx->stringBase + i] = gStrs[i];
        else if (entry.kind == StrEntry::Variable) thread.stringStack[ctx->stringBase + i] = readVariableString(ctx, entry.constant);
    }

    if (thread.valueStack.size() != ctx->valueBase + state->values.size()) thread.valueStack.resize(ctx->valueBase + state->values.size());
    for (uint32_t i = 0; i < state->values.size(); i++) {
        const ValueEntry &entry = state->values[i];
        if (entry.kind == ValueEntry::Opaque || entry.kind == ValueEntry::Like) continue;
        if (entry.kind == ValueEntry::Number) thread.valueStack[ctx->valueBase + i].setNumber(ctx->slots[code.slotValues + i]);
        else thread.valueStack[ctx->valueBase + i] = materializeEntry(ctx, entry, i);
    }
}

void commitVariables(Context *ctx) {
    BytecodeThread &thread = *ctx->thread;
    const Code &code = *ctx->code;

    for (uint32_t k = 0; k < code.promoted.size(); k++) {
        const int64_t kind = ctx->dirty[k];
        if (kind == 0) continue;
        Variable *var = resolveOrCreateVariable(thread, code.promoted[k]);
        if (var == nullptr) continue;
        const double value = ctx->slots[code.slotVariables + k];
        if (kind == 1) var->value = value;
        else if (kind == 2) var->value = Value(value);
        else if (kind == 3) var->value = gShadows[k];
        else if (kind == 4) var->value = std::make_shared<std::string>(std::move(gStringVariables[k]));
        else var->value = ctx->chunk->constants[kind - 5];
    }
}

sljit_sw SLJIT_FUNC jitExit(Context *ctx, const ExitDescriptor *exit) {
    syncStacks(ctx, exit);
    commitVariables(ctx);
    ctx->thread->pc = exit->pc;
    return static_cast<sljit_sw>(exit->result);
}

sljit_sw SLJIT_FUNC jitLeave(Context *ctx, sljit_sw result) {
    commitVariables(ctx);
    return result;
}

bool leanCall(Context *ctx, const CallDescriptor *call) {
    BytecodeThread &thread = *ctx->thread;
    const Code &code = *ctx->code;
    const BytecodeProcedure &proc = ctx->chunk->procedures[call->procedure];
    if (!(thread.withoutScreenRefresh || proc.runWithoutScreenRefresh)) return false;
    if (thread.callStack.size() >= MAX_CALL_DEPTH) return false;

    const ExitDescriptor *state = call->state;
    const EntryDescriptor &entry = code.entries[call->calleeEntry];
    const uint32_t argc = call->argCount;
    if (call->leanCallee < 0) {
        bool lean = entry.numbers == 0 && entry.booleans == 0 && entry.strings == 0 && entry.valueIsNumber.size() == argc;
        for (uint8_t isNumber : entry.valueIsNumber)
            lean = lean && isNumber;
        call->leanCallee = lean ? 1 : 0;
    }
    const uint32_t n = state->numbers, b = state->booleans, v = static_cast<uint32_t>(state->values.size());
    if (call->leanCallee == 0) return false;
    if (argc > v || n > 0xFFFF || b > 0xFFFF || v > 0xFFFF) return false;

    auto &numbers = thread.numberStack;
    auto &booleans = thread.boolStack;
    auto &values = thread.valueStack;
    if (numbers.size() < ctx->numberBase) return false;
    if (booleans.size() < ctx->booleanBase) return false;
    if (values.size() < ctx->valueBase) return false;
    if (thread.stringStack.size() != ctx->stringBase) return false;
    for (uint32_t i = 0; i < v; i++) {
        const ValueEntry::Kind kind = state->values[i].kind;
        if ((kind == ValueEntry::Opaque || kind == ValueEntry::Like) && ctx->valueBase + i >= values.size()) return false;
    }

    const uint32_t kept = v - argc;
    const size_t need = static_cast<size_t>(n) + b + kept;
    if (ctx->saveTop + need > gSave.size()) gSave.resize(std::max<size_t>(gSave.size() * 2, ctx->saveTop + need + 256));

    while (numbers.size() > ctx->numberBase)
        numbers.pop_back();
    for (uint32_t i = 0; i < n; i++)
        numbers.push_back(ctx->slots[i]);
    while (booleans.size() > ctx->booleanBase)
        booleans.pop_back();
    for (uint32_t i = 0; i < b; i++)
        booleans.push_back(bitsOf(ctx->slots[code.slotBooleans + i]) != 0);
    double *valueSlots = ctx->slots + code.slotValues;
    for (uint32_t i = 0; i < v; i++) {
        const ValueEntry &entryKind = state->values[i];
        const size_t index = ctx->valueBase + i;
        if (index >= values.size()) values.emplace_back();
        switch (entryKind.kind) {
        case ValueEntry::Number:
            values[index].setNumber(valueSlots[i]);
            break;
        case ValueEntry::Constant:
            values[index] = ctx->chunk->constants[entryKind.constant];
            break;
        case ValueEntry::Temp:
            values[index] = gTemps[i];
            break;
        default:
            break;
        }
    }
    while (values.size() > ctx->valueBase + v)
        values.pop_back();

    BytecodeCallFrame &frame = thread.callStack.emplace_back();
    frame.procedureIndex = call->procedure;
    frame.returnInstruction = call->callPc + 1;
    frame.stackBase = static_cast<BytecodeIndex>(values.size());
    frame.argCount = argc;
    frame.wantsValue = call->wantsValue;
    frame.previousWarp = thread.withoutScreenRefresh;
    thread.pendingRecursiveCallPc = invalidBytecodeIndex;
    thread.withoutScreenRefresh = thread.withoutScreenRefresh || proc.runWithoutScreenRefresh;
    thread.pc = proc.entryInstruction;

    LeanFrame &record = gLean[ctx->nativeDepth];
    record.returnAndBase = static_cast<uint64_t>(call->callPc + 1) | (static_cast<uint64_t>(frame.stackBase) << 32);
    record.saved = static_cast<uint64_t>(n) | (static_cast<uint64_t>(b) << 16) | (static_cast<uint64_t>(kept) << 32);
    record.flags = static_cast<uint64_t>(argc) | (static_cast<uint64_t>(call->wantsValue) << 32) | (static_cast<uint64_t>(frame.previousWarp) << 33) | kLeanFlag;
    double *save = gSave.data() + ctx->saveTop;
    for (uint32_t i = 0; i < n; i++)
        save[i] = ctx->slots[i];
    save += n;
    for (uint32_t i = 0; i < b; i++)
        save[i] = ctx->slots[code.slotBooleans + i];
    save += b;
    for (uint32_t i = 0; i < kept; i++)
        save[i] = valueSlots[i];
    ctx->saveTop += static_cast<uint32_t>(need);
    ctx->nativeDepth++;

    ctx->numberBase += n;
    ctx->booleanBase += b;
    ctx->valueBase += kept;
    for (uint32_t i = 0; i < argc; i++)
        valueSlots[i] = valueSlots[kept + i];
    ctx->warp = thread.withoutScreenRefresh ? 1 : 0;
    return true;
}

sljit_sw SLJIT_FUNC jitCall(Context *ctx, const CallDescriptor *call) {
    BytecodeThread &thread = *ctx->thread;
    const Code &code = *ctx->code;

    if (call->leanKinds && leanCall(ctx, call)) return 0;
    gLean[ctx->nativeDepth].flags = 0;
    syncStacks(ctx, call->state);

    if (thread.callStack.size() >= MAX_CALL_DEPTH) {
        thread.pc = call->callPc + 1;
        return 1 + static_cast<sljit_sw>(BytecodeJit::Result::Interpret);
    }

    const BytecodeProcedure &proc = ctx->chunk->procedures[call->procedure];

    if (!thread.withoutScreenRefresh && !proc.runWithoutScreenRefresh && thread.pendingRecursiveCallPc != call->callPc) {
        for (const BytecodeCallFrame &existing : thread.callStack) {
            if (existing.procedureIndex == call->procedure) {
                thread.pendingRecursiveCallPc = call->callPc;
                thread.pc = call->callPc;
                return 1 + static_cast<sljit_sw>(BytecodeJit::Result::Yield);
            }
        }
    }
    thread.pendingRecursiveCallPc = invalidBytecodeIndex;

    BytecodeCallFrame frame;
    frame.procedureIndex = call->procedure;
    frame.returnInstruction = call->callPc + 1;
    frame.stackBase = static_cast<BytecodeIndex>(thread.valueStack.size());
    frame.argCount = call->argCount;
    frame.wantsValue = call->wantsValue;
    frame.previousWarp = thread.withoutScreenRefresh;
    thread.callStack.push_back(frame);

    thread.withoutScreenRefresh = thread.withoutScreenRefresh || proc.runWithoutScreenRefresh;
    thread.pc = proc.entryInstruction;

    ctx->nativeDepth++;
    loadFrame(ctx, code.entries[call->calleeEntry]);
    return 0;
}

sljit_sw leanReturn(Context *ctx, const ReturnDescriptor *ret) {
    BytecodeThread &thread = *ctx->thread;
    const Code &code = *ctx->code;
    const ExitDescriptor *state = ret->state;

    const LeanFrame record = gLean[ctx->nativeDepth - 1];
    const BytecodeIndex returnInstruction = static_cast<BytecodeIndex>(record.returnAndBase & 0xFFFFFFFFu), stackBase = static_cast<BytecodeIndex>(record.returnAndBase >> 32);
    const uint32_t argCount = static_cast<uint32_t>(record.flags & 0xFFFFFFFFu);
    const bool wantsValue = (record.flags >> 32) & 1, previousWarp = (record.flags >> 33) & 1;

    enum { NoResult,
           Direct,
           Built } result = NoResult;
    double directNumber = 0.0;
    uint32_t directConstant = 0;
    bool directIsNumber = false;
    Value value;
    if (wantsValue) {
        result = Direct;
        if (ret->isReturn && !state->values.empty()) {
            const ValueEntry &top = state->values.back();
            if (top.kind == ValueEntry::Number) {
                directIsNumber = true;
                directNumber = ctx->slots[code.slotValues + state->values.size() - 1];
            } else if (top.kind == ValueEntry::Constant) {
                directConstant = top.constant;
            } else {
                result = Built;
                value = materializeEntry(ctx, top, static_cast<uint32_t>(state->values.size()) - 1);
            }
        } else {
            directNumber = 0.0;
            directIsNumber = false;
            directConstant = 0xFFFFFFFFu;
        }
    }
    thread.callStack.pop_back();
    thread.withoutScreenRefresh = previousWarp;

    const size_t base = stackBase >= argCount ? static_cast<size_t>(stackBase - argCount) : 0;
    while (thread.valueStack.size() > base)
        thread.valueStack.pop_back();
    if (result == Built) {
        thread.valueStack.push_back(std::move(value));
    } else if (result == Direct) {
        thread.valueStack.emplace_back();
        if (directIsNumber) thread.valueStack.back().setNumber(directNumber);
        else if (directConstant != 0xFFFFFFFFu) thread.valueStack.back() = ctx->chunk->constants[directConstant];
    }
    thread.pc = returnInstruction;

    ctx->nativeDepth--;
    const uint32_t saved = static_cast<uint32_t>(record.saved & 0xFFFF), sb = static_cast<uint32_t>((record.saved >> 16) & 0xFFFF), sv = static_cast<uint32_t>(record.saved >> 32);
    ctx->saveTop -= saved + sb + sv;

    const int32_t entryId = returnInstruction < code.entryIdAt.size() ? code.entryIdAt[returnInstruction] : -1;
    if (entryId < 0) return -1;

    const EntryDescriptor &entry = code.entries[entryId];
    if (entry.numbers == saved && entry.booleans == sb && entry.valueIsNumber.size() == sv + (wantsValue ? 1 : 0)) {
        const double *src = gSave.data() + ctx->saveTop;
        for (uint32_t i = 0; i < saved; i++)
            ctx->slots[i] = src[i];
        src += saved;
        for (uint32_t i = 0; i < sb; i++)
            ctx->slots[code.slotBooleans + i] = src[i];
        src += sb;
        for (uint32_t i = 0; i < sv; i++)
            ctx->slots[code.slotValues + i] = src[i];
        ctx->numberBase -= saved;
        ctx->booleanBase -= sb;
        ctx->valueBase -= sv;
        if (wantsValue && entry.valueIsNumber[sv]) ctx->slots[code.slotValues + sv] = thread.valueStack.back().numberRaw();
        ctx->warp = thread.withoutScreenRefresh ? 1 : 0;
        return entryId;
    }
    loadFrame(ctx, entry);
    return entryId;
}

sljit_sw SLJIT_FUNC jitReturn(Context *ctx, const ReturnDescriptor *ret) {
    BytecodeThread &thread = *ctx->thread;
    const Code &code = *ctx->code;

    if (ctx->nativeDepth > 0 && !thread.callStack.empty() && (gLean[ctx->nativeDepth - 1].flags & kLeanFlag)) return leanReturn(ctx, ret);

    syncStacks(ctx, ret->state);

    if (thread.callStack.empty()) {
        thread.pc = ret->pc;
        return -1;
    }

    const BytecodeCallFrame frame = thread.callStack.back();
    thread.callStack.pop_back();

    Value value;
    if (ret->isReturn && frame.wantsValue && !thread.valueStack.empty()) {
        value = std::move(thread.valueStack.back());
        thread.valueStack.pop_back();
    }
    thread.withoutScreenRefresh = frame.previousWarp;

    const size_t base = frame.stackBase >= frame.argCount ? static_cast<size_t>(frame.stackBase - frame.argCount) : 0;
    if (thread.valueStack.size() > base) thread.valueStack.resize(base);
    if (frame.wantsValue) thread.valueStack.push_back(std::move(value));
    thread.pc = frame.returnInstruction;

    if (ctx->nativeDepth == 0) return -1;
    ctx->nativeDepth--;

    const int32_t entry = frame.returnInstruction < code.entryIdAt.size() ? code.entryIdAt[frame.returnInstruction] : -1;
    if (entry < 0) return -1;
    loadFrame(ctx, code.entries[entry]);
    return entry;
}

void SLJIT_FUNC jitCallValueVoid(Context *ctx, const ValueCallDescriptor *call) {
    const Value value = materializeEntry(ctx, call->item, call->itemPosition);
    call->fn(static_cast<Sprite *>(ctx->sprite), value);
}

void SLJIT_FUNC jitListItem(Context *ctx, const ListOpDescriptor *op) {
    const List *list = listAt(*ctx->thread, op->list);
    Value item;
    if (list != nullptr && !list->items.empty()) {
        const size_t position = listPosition(ctx->slots[ctx->code->slotValues + op->indexPosition], list->items.size());
        if (position < list->items.size()) item = list->items[position];
    }
    ctx->slots[ctx->code->slotValues + op->indexPosition] = item.numberRaw();
    gTemps[op->indexPosition] = std::move(item);
    ctx->tempHigh = std::max(ctx->tempHigh, op->indexPosition + 1);
}

void SLJIT_FUNC jitListReplaceAny(Context *ctx, const ListOpDescriptor *op) {
    List *list = listAt(*ctx->thread, op->list);
    const Value index = materializeEntry(ctx, op->index, op->indexPosition);
    const Value item = materializeEntry(ctx, op->item, op->itemPosition);
    if (list != nullptr) bytecodeListReplace(*list, index, item);
}

void SLJIT_FUNC jitListInsertAny(Context *ctx, const ListOpDescriptor *op) {
    List *list = listAt(*ctx->thread, op->list);
    const Value index = materializeEntry(ctx, op->index, op->indexPosition);
    Value item = materializeEntry(ctx, op->item, op->itemPosition);
    if (list != nullptr) bytecodeListInsert(*list, index, std::move(item));
}

void SLJIT_FUNC jitListDeleteAny(Context *ctx, const ListOpDescriptor *op) {
    List *list = listAt(*ctx->thread, op->list);
    const Value index = materializeEntry(ctx, op->index, op->indexPosition);
    if (list != nullptr) bytecodeListDelete(*list, index);
}

void SLJIT_FUNC jitListItemAny(Context *ctx, const ListOpDescriptor *op) {
    const List *list = listAt(*ctx->thread, op->list);
    const Value index = materializeEntry(ctx, op->item, op->itemPosition);
    Value item;
    if (list != nullptr && !list->items.empty()) {
        const size_t position = bytecodeResolveListIndex(index, list->items.size());
        if (position < list->items.size()) item = list->items[position];
    }
    ctx->slots[ctx->code->slotValues + op->indexPosition] = item.numberRaw();
    gTemps[op->indexPosition] = std::move(item);
    ctx->tempHigh = std::max(ctx->tempHigh, op->indexPosition + 1);
}

void SLJIT_FUNC jitStoreShadow(Context *ctx, const ShadowDescriptor *shadow) {
    gShadows[shadow->variable] = materializeEntry(ctx, shadow->entry, shadow->position);
    ctx->shadowUsed[shadow->variable >> 6] |= uint64_t(1) << (shadow->variable & 63);
}

sljit_sw SLJIT_FUNC jitCompareValues(Context *ctx, const CompareDescriptor *compare) {
    const Value a = materializeEntry(ctx, compare->a, compare->aPosition);
    const Value b = materializeEntry(ctx, compare->b, compare->bPosition);
    switch (compare->kind) {
    case BytecodeCompareKind::Equals:
        return a == b;
    case BytecodeCompareKind::Less:
        return a < b;
    default:
        return a > b;
    }
}

void SLJIT_FUNC jitListReplace(Context *ctx, const ListOpDescriptor *op) {
    List *list = listAt(*ctx->thread, op->list);
    if (list == nullptr || list->items.empty()) return;
    const size_t position = listPosition(ctx->slots[ctx->code->slotValues + op->indexPosition], list->items.size());
    if (position < list->items.size()) list->items[position] = materializeEntry(ctx, op->item, op->itemPosition);
}

void SLJIT_FUNC jitListAdd(Context *ctx, const ListOpDescriptor *op) {
    List *list = listAt(*ctx->thread, op->list);
    Value item = materializeEntry(ctx, op->item, op->itemPosition);
    if (list != nullptr && list->items.size() < BYTECODE_MAX_LIST_ITEMS) list->items.push_back(std::move(item));
}

void SLJIT_FUNC jitListInsert(Context *ctx, const ListOpDescriptor *op) {
    List *list = listAt(*ctx->thread, op->list);
    Value item = materializeEntry(ctx, op->item, op->itemPosition);
    if (list == nullptr) return;
    double index = ctx->slots[ctx->code->slotValues + op->indexPosition];
    if (std::isnan(index)) index = 0.0;
    if (!std::isfinite(index)) return;
    const double position = std::floor(index) - 1;
    if (position < 0 || position > static_cast<double>(list->items.size())) return;
    insertValueAt(list->items, static_cast<size_t>(position), std::move(item));
    if (list->items.size() > BYTECODE_MAX_LIST_ITEMS) list->items.pop_back();
}

void SLJIT_FUNC jitListDelete(Context *ctx, const ListOpDescriptor *op) {
    List *list = listAt(*ctx->thread, op->list);
    if (list == nullptr) return;
    double index = ctx->slots[ctx->code->slotValues + op->indexPosition];
    if (std::isnan(index)) index = 0.0;
    if (!std::isfinite(index)) return;
    const double position = std::floor(index) - 1;
    if (position >= 0 && position < static_cast<double>(list->items.size())) eraseValueAt(list->items, static_cast<size_t>(position));
}

void SLJIT_FUNC jitListClear(Context *ctx, const ListOpDescriptor *op) {
    if (List *list = listAt(*ctx->thread, op->list)) list->items.clear();
}

double SLJIT_FUNC jitListLength(Context *ctx, const ListOpDescriptor *op) {
    const List *list = listAt(*ctx->thread, op->list);
    return list != nullptr ? static_cast<double>(list->items.size()) : 0.0;
}

double SLJIT_FUNC jitListIndexOf(Context *ctx, const ListOpDescriptor *op) {
    const List *list = listAt(*ctx->thread, op->list);
    const Value item = materializeEntry(ctx, op->item, op->itemPosition);
    if (list == nullptr) return 0.0;
    const size_t found = bytecodeFindInList(list->items, item);
    return found < list->items.size() ? static_cast<double>(found + 1) : 0.0;
}

sljit_sw SLJIT_FUNC jitListContains(Context *ctx, const ListOpDescriptor *op) {
    const List *list = listAt(*ctx->thread, op->list);
    const Value item = materializeEntry(ctx, op->item, op->itemPosition);
    if (list == nullptr) return 0;
    return bytecodeFindInList(list->items, item) < list->items.size() ? 1 : 0;
}

void setString(Context *ctx, uint32_t position, std::string &&value);

const std::string &stringOfVariable(Context *ctx, uint32_t k, uint32_t position) {
    const int64_t kind = ctx->dirty[k];
    if (kind == 4) return gStringVariables[k];
    if (kind == 0) {
        const Variable *var = variableAt(*ctx->thread, ctx->code->promoted[k]);
        if (var != nullptr) {
            if (const auto *text = std::get_if<std::shared_ptr<const std::string>>(&var->value)) return **text;
        }
    }
    setString(ctx, position, readVariableString(ctx, k));
    return gStrs[position];
}

const std::string &stringOf(Context *ctx, const StrEntry &entry, uint32_t position) {
    switch (entry.kind) {
    case StrEntry::Const:
        return ctx->chunk->stringConstants[entry.constant];
    case StrEntry::Temp:
        return gStrs[position];
    case StrEntry::Variable:
        return stringOfVariable(ctx, entry.constant, position);
    default:
        return ctx->thread->stringStack[ctx->stringBase + position];
    }
}

std::string readVariableString(Context *ctx, uint32_t k) {
    const Code &code = *ctx->code;
    const int64_t kind = ctx->dirty[k];
    switch (kind) {
    case 0:
        return BlockExecutor::getVariableValueAs<std::string>(variableAt(*ctx->thread, code.promoted[k]));
    case 1:
    case 2:
        return Math::toString(ctx->slots[code.slotVariables + k]);
    case 3:
        return gShadows[k].asString();
    case 4:
        return gStringVariables[k];
    default:
        return ctx->chunk->constants[kind - 5].asString();
    }
}

void setString(Context *ctx, uint32_t position, std::string &&value) {
    gStrs[position] = std::move(value);
    ctx->strHigh = std::max(ctx->strHigh, position + 1);
}

void setStringVariable(Context *ctx, uint32_t k, std::string &&value) {
    gStringVariables[k] = std::move(value);
    gStringAscii[k] = -1;
    ctx->stringVariableUsed[k >> 6] |= uint64_t(1) << (k & 63);
    ctx->dirty[k] = 4;
}

void SLJIT_FUNC jitEnsureStringNumber(Context *ctx, sljit_sw k) {
    ctx->slots[ctx->code->slotVariables + k] = Conversions::stringToNumber(gStringVariables[k]);
}

void SLJIT_FUNC jitStringOp(Context *ctx, const StringOp *op) {
    const Code &code = *ctx->code;
    Sprite *sprite = static_cast<Sprite *>(ctx->sprite);
    const auto number = [&](uint32_t slot) { return ctx->slots[slot]; };
    const auto setNumber = [&](double d) { ctx->slots[op->numberOut] = d; };
    const auto setBoolean = [&](bool b) { ctx->slots[op->numberOut] = fromBits(b ? 1 : 0); };

    switch (op->kind) {
    case StringOp::PushVariable:
        setString(ctx, op->outPosition, readVariableString(ctx, op->variable));
        return;

    case StringOp::StoreVariable:
        setStringVariable(ctx, op->variable, std::string(stringOf(ctx, op->a, op->aPosition)));
        return;

    case StringOp::AppendVariable: {
        const std::string suffix = stringOf(ctx, op->a, op->aPosition);
        if (ctx->dirty[op->variable] == 4) {
            gStringVariables[op->variable] += suffix;
            if (gStringAscii[op->variable] == 1) gStringAscii[op->variable] = -1;
        } else {
            std::string joined = readVariableString(ctx, op->variable);
            joined += suffix;
            setStringVariable(ctx, op->variable, std::move(joined));
        }
        return;
    }

    case StringOp::Compare: {
        const Value a{std::string_view(stringOf(ctx, op->a, op->aPosition))};
        const Value b{std::string_view(stringOf(ctx, op->b, op->bPosition))};
        switch (op->compare) {
        case BytecodeCompareKind::Equals:
            setBoolean(a == b);
            return;
        case BytecodeCompareKind::Less:
            setBoolean(a < b);
            return;
        default:
            setBoolean(a > b);
            return;
        }
    }

    case StringOp::CallStringString:
        setString(ctx, op->outPosition, reinterpret_cast<std::string (*)(const std::string &, const std::string &)>(op->fn)(stringOf(ctx, op->a, op->aPosition), stringOf(ctx, op->b, op->bPosition)));
        return;
    case StringOp::CallStringStringBool:
        setBoolean(reinterpret_cast<bool (*)(const std::string &, const std::string &)>(op->fn)(stringOf(ctx, op->a, op->aPosition), stringOf(ctx, op->b, op->bPosition)));
        return;
    case StringOp::CallStringNumber:
        setNumber(reinterpret_cast<double (*)(const std::string &)>(op->fn)(stringOf(ctx, op->a, op->aPosition)));
        return;
    case StringOp::CallNumberStringString:
        setString(ctx, op->outPosition, reinterpret_cast<std::string (*)(double, const std::string &)>(op->fn)(number(op->numberIn), stringOf(ctx, op->a, op->aPosition)));
        return;
    case StringOp::CallSpriteStringNumber:
        setNumber(reinterpret_cast<double (*)(Sprite *, const std::string &)>(op->fn)(sprite, stringOf(ctx, op->a, op->aPosition)));
        return;
    case StringOp::CallSpriteStringBool:
        setBoolean(reinterpret_cast<bool (*)(Sprite *, const std::string &)>(op->fn)(sprite, stringOf(ctx, op->a, op->aPosition)));
        return;
    case StringOp::CallSpriteString:
        setString(ctx, op->outPosition, reinterpret_cast<std::string (*)(Sprite *)>(op->fn)(sprite));
        return;
    case StringOp::VoidString:
        reinterpret_cast<void (*)(Sprite *, const std::string &)>(op->fn)(sprite, stringOf(ctx, op->a, op->aPosition));
        return;
    case StringOp::VoidStringNumber:
        reinterpret_cast<void (*)(Sprite *, const std::string &, double)>(op->fn)(sprite, stringOf(ctx, op->a, op->aPosition), number(op->numberIn));
        return;

    case StringOp::LetterOfVariable: {
        const double index = std::floor(number(op->numberIn)) - 1;
        std::string letter;
        const int64_t kind = ctx->dirty[op->variable];
        if (kind == 0) {
            const Variable *var = variableAt(*ctx->thread, code.promoted[op->variable]);
            if (var != nullptr) {
                if (const auto *text = std::get_if<std::shared_ptr<const std::string>>(&var->value)) {
                    if (index >= 0 && index < static_cast<double>(Math::utf16Length(*text))) letter = Math::utf16CharAt(*text, static_cast<size_t>(index));
                }
            }
        } else if (kind == 4) {
            const std::string &text = gStringVariables[op->variable];
            if (stringVariableIsAscii(op->variable)) {
                if (index >= 0 && index < static_cast<double>(text.size())) letter.assign(1, text[static_cast<size_t>(index)]);
            } else if (index >= 0 && index < static_cast<double>(Math::utf16Length(std::string_view(text)))) {
                letter = Math::utf16CharAt(std::string_view(text), static_cast<size_t>(index));
            }
        }
        setString(ctx, op->outPosition, std::move(letter));
        return;
    }
    case StringOp::LengthOfVariable: {
        size_t units = 0;
        const int64_t kind = ctx->dirty[op->variable];
        if (kind == 0) {
            const Variable *var = variableAt(*ctx->thread, code.promoted[op->variable]);
            if (var != nullptr) {
                if (const auto *text = std::get_if<std::shared_ptr<const std::string>>(&var->value)) units = Math::utf16Length(*text);
            }
        } else if (kind == 4) {
            units = stringVariableIsAscii(op->variable) ? gStringVariables[op->variable].size() : Math::utf16Length(std::string_view(gStringVariables[op->variable]));
        }
        setNumber(static_cast<double>(units));
        return;
    }

    case StringOp::LetterOf: {
        const Value text = materializeEntry(ctx, op->value, op->valuePosition);
        const double index = std::floor(number(op->numberIn)) - 1;
        std::string letter;
        if (index >= 0 && index < static_cast<double>(text.utf16Length())) letter = text.utf16CharAt(static_cast<size_t>(index));
        setString(ctx, op->outPosition, std::move(letter));
        return;
    }
    case StringOp::LengthOf:
        setNumber(static_cast<double>(materializeEntry(ctx, op->value, op->valuePosition).utf16Length()));
        return;

    case StringOp::ValueToString:
        setString(ctx, op->outPosition, materializeEntry(ctx, op->value, op->valuePosition).get<std::string>());
        return;
    case StringOp::StringToValue: {
        Value value{std::string_view(stringOf(ctx, op->a, op->aPosition))};
        ctx->slots[code.slotValues + op->outPosition] = value.numberRaw();
        gTemps[op->outPosition] = std::move(value);
        ctx->tempHigh = std::max(ctx->tempHigh, op->outPosition + 1);
        return;
    }
    }
}

double SLJIT_FUNC jitRepeatRemaining(double counter) {
    if (std::isnan(counter)) counter = 0.0;
    return std::round(counter) - 1.0;
}

sljit_sw SLJIT_FUNC jitCompareUnordered(double a, double b, sljit_sw kind) {
    const bool nan1 = std::isnan(a), nan2 = std::isnan(b);
    switch (static_cast<BytecodeCompareKind>(kind)) {
    case BytecodeCompareKind::Equals:
        return nan1 && nan2;
    case BytecodeCompareKind::Less:
        return nan2 && !nan1;
    default:
        return nan1 && !nan2;
    }
}

double SLJIT_FUNC jitMathOp(double value, sljit_sw op) {
    return bytecodeMathOpApply(static_cast<BytecodeMathOp>(op), value);
}

sljit_sw SLJIT_FUNC jitPreempt(Context *ctx) {
    if (!(Scratch::warpTimer && ctx->thread->withoutScreenRefresh)) return 0;
    if (!*ctx->timerStarted) {
        ctx->timer->start();
        *ctx->timerStarted = true;
        return 0;
    }
    return ctx->timer->getTimeMs() > 500 ? 1 : 0;
}

bool isJump(BytecodeOp op) {
    switch (op) {
    case BytecodeOp::Jump:
    case BytecodeOp::JumpIfFalse:
    case BytecodeOp::JumpIfTrue:
    case BytecodeOp::JumpIfNumCompareFalse:
    case BytecodeOp::JumpIfNumCompareTrue:
    case BytecodeOp::RepeatCheck:
    case BytecodeOp::ForEachCheck:
        return true;
    default:
        return false;
    }
}

bool isCall(BytecodeOp op) {
    return op == BytecodeOp::CallProcedure || op == BytecodeOp::CallProcedureValue;
}

bool isTerminal(BytecodeOp op) {
    return op == BytecodeOp::Halt || op == BytecodeOp::Return || op == BytecodeOp::EscapeToLegacyThread;
}

enum class NativeShape {
    None,
    Add,
    Subtract,
    Multiply,
    Divide,
    NumberNumber,
    Number,
    SpriteNumber,
    And,
    Or,
    Not,
    BoolBool,
    BoolUnary,
    SpriteBoolean,
    VoidNone,
    VoidNumber,
    VoidNumberNumber,
    VoidValue,
    StringStringString,
    StringStringBool,
    StringNumber,
    NumberStringString,
    SpriteStringNumber,
    SpriteStringBool,
    SpriteString,
    VoidString,
    VoidStringNumber,
};

struct Planner {
    const BytecodeChunk &chunk;
    const BytecodeLinkTable &link;
    BytecodeAnalysis &analysis;
    size_t count;

    std::vector<uint8_t> supported;
    std::vector<uint8_t> needsExit;
    std::vector<int32_t> entryOf;
    std::vector<BytecodeIndex> entryPcs;
    std::unordered_map<uint8_t, uint32_t> loopBlockers;
    std::map<std::pair<int, uint8_t>, uint32_t> procedureBlockers;
    std::vector<uint8_t> visible;
    std::vector<std::vector<BytecodeIndex>> entryReads;

    static constexpr uint32_t kMinRegionInstructions = 40;

    Planner(const BytecodeChunk &c, const BytecodeLinkTable &l, BytecodeAnalysis &a)
        : chunk(c), link(l), analysis(a), count(c.instructions.size()) {}

    const std::string &opcodeName(BytecodeIndex slot) const {
        static const std::string empty;
        if (slot >= chunk.opcodeSlots.size() || chunk.opcodeSlots[slot] >= chunk.strings.size()) return empty;
        return chunk.strings[chunk.opcodeSlots[slot]];
    }

    NativeShape nativeShape(const BytecodeInstruction &instr) const {
        const BytecodeIndex slot = instr.a;
        if (slot >= chunk.paramKindsStart.size() || slot >= link.positional.size()) return NativeShape::None;
        const BytecodeIndex start = chunk.paramKindsStart[slot];
        const BytecodeIndex args = chunk.paramKindsCount[slot];
        const BytecodePositionalFn &fn = link.positional[slot];
        const auto kind = [&](BytecodeIndex i) { return chunk.paramKinds[start + i]; };
        using K = BytecodeStackKind;

        if (instr.op == BytecodeOp::CallNativeNumber) {
            if (args == 2 && kind(0) == K::Number && kind(1) == K::Number && fn.nn_n) {
                const std::string &name = opcodeName(slot);
                if (name == "operator_add") return NativeShape::Add;
                if (name == "operator_subtract") return NativeShape::Subtract;
                if (name == "operator_multiply") return NativeShape::Multiply;
                if (name == "operator_divide") return NativeShape::Divide;
                return NativeShape::NumberNumber;
            }
            if (args == 1 && kind(0) == K::Number && fn.n_n) return NativeShape::Number;
            if (args == 0 && fn.sprite_n) return NativeShape::SpriteNumber;
            if (args == 1 && kind(0) == K::String && fn.s_n) return NativeShape::StringNumber;
            if (args == 1 && kind(0) == K::String && fn.spriteS_n) return NativeShape::SpriteStringNumber;
        } else if (instr.op == BytecodeOp::CallNativeString) {
            if (args == 2 && kind(0) == K::String && kind(1) == K::String && fn.ss_s) return NativeShape::StringStringString;
            if (args == 2 && kind(0) == K::Number && kind(1) == K::String && fn.ns_s) return NativeShape::NumberStringString;
            if (args == 0 && fn.sprite_s) return NativeShape::SpriteString;
        } else if (instr.op == BytecodeOp::CallNativeBoolean) {
            if (args == 2 && kind(0) == K::Boolean && kind(1) == K::Boolean && fn.bb_b) {
                const std::string &name = opcodeName(slot);
                if (name == "operator_and") return NativeShape::And;
                if (name == "operator_or") return NativeShape::Or;
                return NativeShape::BoolBool;
            }
            if (args == 1 && kind(0) == K::Boolean && fn.b_b) {
                return opcodeName(slot) == "operator_not" ? NativeShape::Not : NativeShape::BoolUnary;
            }
            if (args == 0 && fn.sprite_b) return NativeShape::SpriteBoolean;
            if (args == 2 && kind(0) == K::String && kind(1) == K::String && fn.ss_b) return NativeShape::StringStringBool;
            if (args == 1 && kind(0) == K::String && fn.spriteS_b) return NativeShape::SpriteStringBool;
        } else if (instr.op == BytecodeOp::CallNative && voidCallAllowed(opcodeName(slot))) {
            if (args == 1 && kind(0) == K::String && fn.s_void) return NativeShape::VoidString;
            if (args == 2 && kind(0) == K::String && kind(1) == K::Number && fn.sn_void) return NativeShape::VoidStringNumber;
            if (args == 0 && fn.voidFn) return NativeShape::VoidNone;
            if (args == 1 && kind(0) == K::Number && fn.n_void) return NativeShape::VoidNumber;
            if (args == 2 && kind(0) == K::Number && kind(1) == K::Number && fn.nn_void) return NativeShape::VoidNumberNumber;
            if (args == 1 && kind(0) == K::Value && fn.v_void) return NativeShape::VoidValue;
        }
        return NativeShape::None;
    }

    static bool voidCallAllowed(const std::string &opcode) {
        for (const char *prefix : {"motion_", "looks_", "pen_", "music_", "sound_"}) {
            if (opcode.rfind(prefix, 0) == 0) return true;
        }
        return opcode == "control_incr_counter" || opcode == "control_clear_counter" || opcode == "sensing_resettimer";
    }

    bool followsConstant(BytecodeIndex pc) const {
        return pc > 0 && !analysis.isBlockStart(pc) && chunk.instructions[pc - 1].op == BytecodeOp::PushConstValue &&
               chunk.instructions[pc - 1].a < chunk.constants.size();
    }

    bool topValueIsNumber(BytecodeIndex pc) const {
        BytecodeAbstractState state;
        if (!analysis.stateBefore(pc, state) || state.values.empty()) return false;
        return state.values.back() == BytecodeValueType::Number;
    }

    static bool numericLike(BytecodeValueType type) {
        return type == BytecodeValueType::Number || type == BytecodeValueType::NumericString || type == BytecodeValueType::NumberLike;
    }

    bool topValuesAreNumeric(BytecodeIndex pc, size_t how_many) const {
        BytecodeAbstractState state;
        if (!analysis.stateBefore(pc, state) || state.values.size() < how_many) return false;
        for (size_t i = 0; i < how_many; i++) {
            if (!numericLike(state.values[state.values.size() - 1 - i])) return false;
        }
        return true;
    }

    bool consumedInBlock(BytecodeIndex pc, int64_t variable, int positionOffset = 0) const {
        BytecodeAbstractState pushed;
        if (!analysis.stateBefore(pc, pushed)) return false;
        if (static_cast<int>(pushed.values.size()) + positionOffset < 0) return false;
        const size_t position = pushed.values.size() + positionOffset;
        for (BytecodeIndex q = pc + 1; q < count; q++) {
            const BytecodeInstruction &between = chunk.instructions[q - 1];
            if (variable >= 0) {
                if (between.op == BytecodeOp::ItemOfList) {
                    BytecodeAbstractState before;
                    if (analysis.stateBefore(q - 1, before) && before.values.size() == position + 1) return true;
                }
                switch (between.op) {
                case BytecodeOp::StoreVarNumber:
                case BytecodeOp::StoreVarString:
                case BytecodeOp::StoreVarBoolean:
                case BytecodeOp::StoreVarColor:
                case BytecodeOp::StoreVarValue:
                case BytecodeOp::AppendVarString:
                case BytecodeOp::ChangeVarNumber:
                    if (between.a == static_cast<BytecodeIndex>(variable)) return false;
                    break;
                case BytecodeOp::ForEachCheck:
                    if (between.b == static_cast<BytecodeIndex>(variable)) return false;
                    break;
                default:
                    break;
                }
            }
            BytecodeAbstractState state;
            if (!analysis.stateBefore(q, state)) return false;
            if (state.values.size() <= position) return true;
            if (analysis.isBlockStart(q)) return false;
        }
        return false;
    }

    bool listValid(const BytecodeInstruction &instr) const { return instr.a < chunk.listSlots.size(); }

    bool stringConsumedInBlock(BytecodeIndex pc, int offset) const {
        BytecodeAbstractState before;
        if (!analysis.stateBefore(pc, before)) return false;
        const int position = static_cast<int>(before.strings) + offset;
        if (position < 0) return false;
        for (BytecodeIndex q = pc + 1; q < count; q++) {
            BytecodeAbstractState state;
            if (!analysis.stateBefore(q, state)) return false;
            if (static_cast<int>(state.strings) <= position) return true;
            if (analysis.isBlockStart(q)) return false;
        }
        return false;
    }

    bool variableBorrowable(BytecodeIndex pc) const {
        const BytecodeInstruction &push = chunk.instructions[pc];
        BytecodeAbstractState before;
        if (!analysis.stateBefore(pc, before)) return false;
        const int position = static_cast<int>(before.strings);
        for (BytecodeIndex q = pc + 1; q < count; q++) {
            BytecodeAbstractState state;
            if (!analysis.stateBefore(q, state)) return false;
            if (static_cast<int>(state.strings) <= position) return true;
            if (analysis.isBlockStart(q) || !supported[q]) return false;

            const BytecodeInstruction &instr = chunk.instructions[q];
            switch (instr.op) {
            case BytecodeOp::StoreVarNumber:
            case BytecodeOp::StoreVarString:
            case BytecodeOp::StoreVarValue:
            case BytecodeOp::AppendVarString:
            case BytecodeOp::ChangeVarNumber:
                if (instr.a == push.a) return false;
                break;
            case BytecodeOp::CallProcedure:
            case BytecodeOp::CallProcedureValue:
            case BytecodeOp::CallNative:
            case BytecodeOp::Yield:
            case BytecodeOp::ForEachCheck:
            case BytecodeOp::RepeatCheck:
                return false;
            default:
                break;
            }
        }
        return false;
    }

    static int stringArguments(NativeShape shape) {
        switch (shape) {
        case NativeShape::StringStringString:
        case NativeShape::StringStringBool:
            return 2;
        case NativeShape::StringNumber:
        case NativeShape::NumberStringString:
        case NativeShape::SpriteStringNumber:
        case NativeShape::SpriteStringBool:
        case NativeShape::VoidString:
        case NativeShape::VoidStringNumber:
            return 1;
        default:
            return 0;
        }
    }

    static bool leavesString(NativeShape shape) {
        return shape == NativeShape::StringStringString || shape == NativeShape::NumberStringString || shape == NativeShape::SpriteString;
    }

    static bool isNumericConstant(const Value &value) {
        return value.isDouble() || (value.isString() && BytecodeAnalysis::isCanonicalNumericString(value.asString()));
    }

    bool opSupported(BytecodeIndex pc) const {
        const BytecodeInstruction &instr = chunk.instructions[pc];
        const size_t variableCount = chunk.variableSlots.size();
        switch (instr.op) {
        case BytecodeOp::Nop:
        case BytecodeOp::PushConstNumber:
        case BytecodeOp::PushConstBoolean:
        case BytecodeOp::PopNumber:
        case BytecodeOp::PopBoolean:
        case BytecodeOp::ConvertNumberToValue:
        case BytecodeOp::Jump:
        case BytecodeOp::JumpIfFalse:
        case BytecodeOp::JumpIfTrue:
        case BytecodeOp::NumEquals:
        case BytecodeOp::NumLess:
        case BytecodeOp::NumGreater:
        case BytecodeOp::JumpIfNumCompareFalse:
        case BytecodeOp::JumpIfNumCompareTrue:
        case BytecodeOp::MathOp:
        case BytecodeOp::Yield:
            return true;
        case BytecodeOp::PushVarNumber:
        case BytecodeOp::StoreVarNumber:
        case BytecodeOp::ChangeVarNumber:
            return instr.a < variableCount;
        case BytecodeOp::RepeatCheck:
            return topValueIsNumber(pc);
        case BytecodeOp::ForEachCheck:
            return instr.b < variableCount && topValuesAreNumeric(pc, 2);
        case BytecodeOp::PopValue:
            return true;
        case BytecodeOp::PushConstValue: {
            if (instr.a >= chunk.constants.size()) return false;
            if (chunk.constants[instr.a].isDouble()) return true;
            if (pc + 1 < count && !analysis.isBlockStart(pc + 1)) {
                const BytecodeOp next = chunk.instructions[pc + 1].op;
                if (next == BytecodeOp::ConvertValueToNumber || next == BytecodeOp::StoreVarValue) return true;
            }
            return consumedInBlock(pc, -1);
        }
        case BytecodeOp::PushVarValue:
            return instr.a < variableCount && consumedInBlock(pc, instr.a);

        case BytecodeOp::ItemOfList:
            return listValid(instr) && consumedInBlock(pc, -1, -1);
        case BytecodeOp::ReplaceInList:
        case BytecodeOp::DeleteFromList:
        case BytecodeOp::InsertInList:
            return listValid(instr);
        case BytecodeOp::AddToList:
        case BytecodeOp::ItemNumOfList:
        case BytecodeOp::ListContainsItem: {
            BytecodeAbstractState state;
            return listValid(instr) && analysis.stateBefore(pc, state) && !state.values.empty();
        }
        case BytecodeOp::LengthOfList:
        case BytecodeOp::DeleteAllOfList:
            return listValid(instr);

        case BytecodeOp::CallProcedure:
        case BytecodeOp::CallProcedureValue: {
            if (instr.a >= chunk.procedures.size()) return false;
            const BytecodeProcedure &proc = chunk.procedures[instr.a];
            BytecodeAbstractState state;
            return proc.argCount == instr.b && proc.entryInstruction < count && analysis.stateBefore(pc, state) && state.values.size() >= instr.b;
        }
        case BytecodeOp::Return: {
            BytecodeAbstractState state;
            return analysis.stateBefore(pc, state) && !state.values.empty();
        }
        case BytecodeOp::Halt:
            return true;
        case BytecodeOp::PushArg: {
            const int procedure = analysis.procedureAt(pc);
            return procedure >= 0 && instr.a < chunk.procedures[procedure].argCount && consumedInBlock(pc, -1);
        }
        case BytecodeOp::ValueEquals:
        case BytecodeOp::ValueLess:
        case BytecodeOp::ValueGreater:
        case BytecodeOp::ConvertValueToNumber:
            return true;
        case BytecodeOp::StoreVarValue:
            return instr.a < variableCount;
        case BytecodeOp::CallNative:
        case BytecodeOp::CallNativeNumber:
        case BytecodeOp::CallNativeString:
        case BytecodeOp::CallNativeBoolean: {
            const NativeShape shape = nativeShape(instr);
            if (shape == NativeShape::None) return false;
            return !leavesString(shape) || stringConsumedInBlock(pc, -stringArguments(shape));
        }

        case BytecodeOp::PushConstString:
            return instr.a < chunk.stringConstants.size() && stringConsumedInBlock(pc, 0);
        case BytecodeOp::PushVarString:
            return instr.a < variableCount && stringConsumedInBlock(pc, 0);
        case BytecodeOp::StoreVarString:
        case BytecodeOp::AppendVarString:
            return instr.a < variableCount;
        case BytecodeOp::PopString:
        case BytecodeOp::StrEquals:
        case BytecodeOp::StrLess:
        case BytecodeOp::StrGreater:
        case BytecodeOp::LengthOf:
            return true;
        case BytecodeOp::LengthOfVar:
            return instr.a < variableCount;
        case BytecodeOp::ConvertStringToValue:
            return consumedInBlock(pc, -1);
        case BytecodeOp::ConvertValueToString:
        case BytecodeOp::LetterOf:
            return stringConsumedInBlock(pc, 0);
        case BytecodeOp::LetterOfVar:
            return instr.a < variableCount && stringConsumedInBlock(pc, 0);
        default:
            return false;
        }
    }

    void plan(const std::vector<BytecodeIndex> &roots) {
        supported.assign(count, 0);
        needsExit.assign(count, 0);
        entryOf.assign(count, -1);

        for (BytecodeIndex pc = 0; pc < count; pc++) {
            if (analysis.isReachable(pc)) supported[pc] = opSupported(pc) ? 1 : 0;
        }
        limitPromotedVariables();

        for (BytecodeIndex pc = 0; pc < count; pc++) {
            const BytecodeInstruction &instr = chunk.instructions[pc];
            if (!isJump(instr.op) || instr.a >= count || instr.a > pc) continue;
            for (BytecodeIndex i = instr.a; i <= pc; i++) {
                if (analysis.isReachable(i) && !supported[i]) {
                    loopBlockers[static_cast<uint8_t>(chunk.instructions[i].op)]++;
                    procedureBlockers[{analysis.procedureAt(i), static_cast<uint8_t>(chunk.instructions[i].op)}]++;
                }
            }
        }

        for (bool changed = true; changed;) {
            changed = false;
            for (BytecodeIndex pc = 0; pc < count; pc++) {
                if (!supported[pc] || !isCall(chunk.instructions[pc].op)) continue;
                if (!supported[chunk.procedures[chunk.instructions[pc].a].entryInstruction]) {
                    supported[pc] = 0;
                    changed = true;
                }
            }
            for (BytecodeIndex pc = 0; pc < count; pc++) {
                const BytecodeInstruction &instr = chunk.instructions[pc];
                if (!isJump(instr.op) || instr.a >= count || instr.a > pc) continue;
                bool allSupported = true;
                for (BytecodeIndex i = instr.a; i <= pc; i++) {
                    if (analysis.isReachable(i) && !supported[i]) allSupported = false;
                }
                if (allSupported) continue;
                for (BytecodeIndex i = instr.a; i <= pc; i++) {
                    if (supported[i]) {
                        supported[i] = 0;
                        changed = true;
                    }
                }
            }
        }

        const auto successors = [&](BytecodeIndex pc, std::vector<BytecodeIndex> &out) {
            const BytecodeInstruction &instr = chunk.instructions[pc];
            if (isTerminal(instr.op)) return;
            if (isJump(instr.op) && instr.a < count) out.push_back(instr.a);
            if (instr.op != BytecodeOp::Jump && pc + 1 < count) out.push_back(pc + 1);
        };

        std::vector<BytecodeIndex> next;
        for (BytecodeIndex pc = 0; pc < count; pc++) {
            if (!analysis.isReachable(pc)) continue;
            next.clear();
            successors(pc, next);
            if (supported[pc]) {
                for (BytecodeIndex s : next) {
                    if (analysis.isReachable(s) && !supported[s]) needsExit[s] = 1;
                }
                const BytecodeInstruction &here = chunk.instructions[pc];
                if (isJump(here.op) && here.a <= pc && here.a < count && supported[here.a]) addEntry(here.a);
                if (chunk.instructions[pc].op == BytecodeOp::Yield && pc + 1 < count && supported[pc + 1]) addEntry(pc + 1);
                if (isCall(here.op) && pc + 1 < count && supported[pc + 1]) addEntry(pc + 1);
            } else {
                for (BytecodeIndex s : next) {
                    if (analysis.isReachable(s) && supported[s]) addEntry(s);
                }
            }
        }
        for (BytecodeIndex root : roots) {
            if (root < count && supported[root]) addEntry(root);
        }

        measureEntries();
    }

    void measureRegion(BytecodeIndex entry, uint32_t &instructions, bool &hasLoop, std::vector<BytecodeIndex> &reads) const {
        instructions = 0;
        hasLoop = false;
        std::vector<uint8_t> seen(count, 0);
        std::vector<BytecodeIndex> stack{entry};
        while (!stack.empty()) {
            const BytecodeIndex pc = stack.back();
            stack.pop_back();
            if (pc >= count || seen[pc] || !supported[pc]) continue;
            seen[pc] = 1;
            instructions++;
            const BytecodeInstruction &instr = chunk.instructions[pc];
            if (instr.op == BytecodeOp::PushVarNumber || instr.op == BytecodeOp::ChangeVarNumber || instr.op == BytecodeOp::PushVarValue) {
                if (std::find(reads.begin(), reads.end(), instr.a) == reads.end()) reads.push_back(instr.a);
            }
            if (isTerminal(instr.op)) continue;
            if (isJump(instr.op) && instr.a < count) {
                if (instr.a <= pc) hasLoop = true;
                stack.push_back(instr.a);
            }
            if (isCall(instr.op)) stack.push_back(chunk.procedures[instr.a].entryInstruction);
            if (instr.op != BytecodeOp::Jump) stack.push_back(pc + 1);
        }
    }

    int64_t promotedVariable(const BytecodeInstruction &instr) const {
        switch (instr.op) {
        case BytecodeOp::PushVarNumber:
        case BytecodeOp::StoreVarNumber:
        case BytecodeOp::ChangeVarNumber:
        case BytecodeOp::StoreVarValue:
        case BytecodeOp::PushVarValue:
        case BytecodeOp::PushVarString:
        case BytecodeOp::StoreVarString:
        case BytecodeOp::AppendVarString:
        case BytecodeOp::LetterOfVar:
        case BytecodeOp::LengthOfVar:
            return instr.a;
        case BytecodeOp::ForEachCheck:
            return instr.b;
        default:
            return -1;
        }
    }

    void limitPromotedVariables() {
        std::unordered_map<int64_t, uint32_t> uses;
        for (BytecodeIndex pc = 0; pc < count; pc++) {
            if (!supported[pc]) continue;
            const int64_t variable = promotedVariable(chunk.instructions[pc]);
            if (variable >= 0) uses[variable]++;
        }
        if (uses.size() <= kMaxPromoted) return;

        std::vector<std::pair<uint32_t, int64_t>> ranked;
        for (const auto &[variable, n] : uses)
            ranked.push_back({n, variable});
        std::sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
        std::unordered_map<int64_t, bool> kept;
        for (size_t i = 0; i < kMaxPromoted; i++)
            kept[ranked[i].second] = true;

        for (BytecodeIndex pc = 0; pc < count; pc++) {
            if (!supported[pc]) continue;
            const int64_t variable = promotedVariable(chunk.instructions[pc]);
            if (variable >= 0 && !kept.count(variable)) supported[pc] = 0;
        }
    }

    void measureEntries() {
        entryReads.clear();
        visible.clear();
        for (BytecodeIndex pc : entryPcs) {
            uint32_t instructions;
            bool hasLoop;
            std::vector<BytecodeIndex> reads;
            measureRegion(pc, instructions, hasLoop, reads);
            entryReads.push_back(std::move(reads));
            visible.push_back(hasLoop || instructions >= kMinRegionInstructions ? 1 : 0);
        }
    }

    void addEntry(BytecodeIndex pc) {
        if (entryOf[pc] >= 0) return;
        entryOf[pc] = static_cast<int32_t>(entryPcs.size());
        entryPcs.push_back(pc);
    }
};

struct AbsValue {
    ValueEntry::Kind kind = ValueEntry::Opaque;
    uint32_t constant = 0;
};

struct AbsState {
    uint32_t numbers = 0;
    uint32_t booleans = 0;
    std::vector<AbsValue> values;
    std::vector<StrEntry> strings;
};

class Emitter {
  public:
    Emitter(const BytecodeChunk &chunk, const BytecodeLinkTable &link, BytecodeAnalysis &analysis, Planner &planner, Code &code)
        : chunk_(chunk), link_(link), analysis_(analysis), plan_(planner), code_(code) {}

    bool emit();

    bool giveUp(const std::string &why) const {
        if (const char *log = std::getenv("SE_JIT_LOG"); log != nullptr && std::string(log) != "0") Log::log("[jit] not compiled: " + why);
        return false;
    }

  private:
    const BytecodeChunk &chunk_;
    const BytecodeLinkTable &link_;
    BytecodeAnalysis &analysis_;
    Planner &plan_;
    Code &code_;

    sljit_compiler *c_ = nullptr;
    std::vector<sljit_label *> labels_;
    std::vector<std::pair<sljit_jump *, BytecodeIndex>> pending_;

    struct Region {
        BytecodeIndex head = 0, tail = 0;
        std::vector<uint32_t> slots;
        sljit_label *body = nullptr;
    };
    static constexpr int kFirstPin = 4;
    static constexpr size_t kMaxPins = 8;
    std::deque<Region> regions_;
    std::vector<int32_t> regionOf_;
    const Region *active_ = nullptr;
    std::vector<std::pair<sljit_jump *, const Region *>> pendingBody_;
    bool pinViolation_ = false;
    const bool poisonPins_ = [] {
        const char *poison = std::getenv("SE_JIT_POISON");
        return poison != nullptr && std::string(poison) != "0";
    }();
    std::vector<sljit_jump *> exitJumps_;
    std::vector<sljit_jump *> leaveJumps_;
    std::vector<std::function<void()>> cold_;
    std::vector<int32_t> promotedIndex_;
    std::vector<bool> stringWritten_;

    AbsState state_;
    uint32_t maxNumbers_ = 0, maxBooleans_ = 0, maxValues_ = 0, maxStrings_ = 0;

    static constexpr sljit_sw slotsOffset() { return static_cast<sljit_sw>(offsetof(Context, slots)); }
    static sljit_sw slotOffset(uint32_t slot) { return slotsOffset() + static_cast<sljit_sw>(slot) * 8; }
    static sljit_sw dirtyOffset(uint32_t k) { return static_cast<sljit_sw>(offsetof(Context, dirty)) + static_cast<sljit_sw>(k) * 8; }

    uint32_t numberSlot(uint32_t depth) const { return depth; }
    uint32_t booleanSlot(uint32_t depth) const { return code_.slotBooleans + depth; }
    uint32_t valueSlot(uint32_t depth) const { return code_.slotValues + depth; }
    uint32_t variableSlot(uint32_t k) const { return code_.slotVariables + k; }

    int pinOf(uint32_t slot) {
        if (active_ == nullptr) return -1;
        for (size_t i = 0; i < active_->slots.size(); i++) {
            if (active_->slots[i] == slot) return kFirstPin + static_cast<int>(i);
        }
        return -1;
    }
    bool insideActive(BytecodeIndex target) const { return active_ != nullptr && target >= active_->head && target <= active_->tail; }

    void emitSpill() {
        if (active_ == nullptr) return;
        for (size_t i = 0; i < active_->slots.size(); i++)
            sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), slotOffset(active_->slots[i]), SLJIT_FR(kFirstPin + static_cast<int>(i)), 0);
    }
    void emitReload() {
        if (active_ == nullptr) return;
        for (size_t i = 0; i < active_->slots.size(); i++)
            sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_FR(kFirstPin + static_cast<int>(i)), 0, SLJIT_MEM1(SLJIT_S0), slotOffset(active_->slots[i]));
    }

    void addCold(std::function<void()> fn) {
        const Region *region = active_;
        cold_.push_back([this, region, fn = std::move(fn)]() {
            active_ = region;
            fn();
            active_ = nullptr;
        });
    }

    void pendJump(sljit_jump *jump, BytecodeIndex target) {
        if (active_ != nullptr && !insideActive(target)) {
            addCold([this, jump, target]() {
                sljit_set_label(jump, sljit_emit_label(c_));
                emitSpill();
                pending_.push_back({sljit_emit_jump(c_, SLJIT_JUMP), target});
            });
        } else if (active_ != nullptr && target == active_->head) {
            pendingBody_.push_back({jump, active_});
        } else {
            pending_.push_back({jump, target});
        }
    }
    void pendUnconditional(BytecodeIndex target) {
        if (active_ != nullptr && !insideActive(target)) emitSpill();
        pendJump(sljit_emit_jump(c_, SLJIT_JUMP), target);
    }

    void loadF(sljit_s32 reg, uint32_t slot) {
        if (const int pin = pinOf(slot); pin >= 0) sljit_emit_fop1(c_, SLJIT_MOV_F64, reg, 0, SLJIT_FR(pin), 0);
        else sljit_emit_fop1(c_, SLJIT_MOV_F64, reg, 0, SLJIT_MEM1(SLJIT_S0), slotOffset(slot));
    }
    void storeF(uint32_t slot, sljit_s32 reg) {
        if (const int pin = pinOf(slot); pin >= 0) sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_FR(pin), 0, reg, 0);
        else sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), slotOffset(slot), reg, 0);
    }
    void loadW(sljit_s32 reg, uint32_t slot) {
        if (pinOf(slot) >= 0) pinViolation_ = true;
        sljit_emit_op1(c_, SLJIT_MOV, reg, 0, SLJIT_MEM1(SLJIT_S0), slotOffset(slot));
    }
    void storeW(uint32_t slot, sljit_s32 reg) {
        if (pinOf(slot) >= 0) pinViolation_ = true;
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), slotOffset(slot), reg, 0);
    }
    void storeImmW(uint32_t slot, sljit_sw value) {
        if (pinOf(slot) >= 0) pinViolation_ = true;
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), slotOffset(slot), SLJIT_IMM, value);
    }
    void storeImmF(uint32_t slot, double value) {
        sljit_emit_fset64(c_, SLJIT_FR0, value);
        storeF(slot, SLJIT_FR0);
    }

    void nanToZero(sljit_s32 reg) {
        sljit_jump *notNan = sljit_emit_fcmp(c_, SLJIT_ORDERED_EQUAL, reg, 0, reg, 0);
        sljit_emit_fset64(c_, reg, 0.0);
        sljit_set_label(notNan, sljit_emit_label(c_));
    }

    void callHelper(void *fn, sljit_s32 argTypes) {
        emitSpill();
        sljit_emit_icall(c_, SLJIT_CALL, argTypes, SLJIT_IMM, reinterpret_cast<sljit_sw>(fn));
        if (poisonPins_ && active_ != nullptr) {
            for (size_t i = 0; i < active_->slots.size(); i++)
                sljit_emit_fset64(c_, SLJIT_FR(kFirstPin + static_cast<int>(i)), -123456.789);
        }
        emitReload();
    }
    void findRegions();

    void resetStateFromAnalysis(BytecodeIndex pc);
    AbsState stateAfterPop(const AbsState &s) const { return s; }
    void emitExitAt(BytecodeIndex pc, uint32_t result, const AbsState &state);
    void emitJumpTo(sljit_jump *jump, BytecodeIndex target, const AbsState &stateAtTarget, bool backEdge);
    void emitBackEdgeThenJump(BytecodeIndex target, const AbsState &stateAtTarget);
    void emitInstruction(BytecodeIndex pc);
    void emitCompareValue(BytecodeCompareKind kind, uint32_t a, uint32_t b);

    static ValueEntry describe(const AbsValue &v) {
        ValueEntry e;
        e.kind = v.kind;
        e.constant = v.constant;
        return e;
    }

    StringOp &newStringOp(StringOp::Kind kind) {
        StringOp &op = code_.stringOps.emplace_back();
        op.kind = kind;
        return op;
    }

    void callStringOp(StringOp &op) {
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&op));
        callHelper(reinterpret_cast<void *>(&jitStringOp), SLJIT_ARGS2V(P, P));
    }

    void ensureVariableNumber(BytecodeIndex variable) {
        if (!stringWritten_[variable]) return;
        const uint32_t k = promoted(variable);
        sljit_jump *holdsString = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 4);
        sljit_label *back = sljit_emit_label(c_);
        addCold([this, holdsString, back, k]() {
            sljit_set_label(holdsString, sljit_emit_label(c_));
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, k);
            callHelper(reinterpret_cast<void *>(&jitEnsureStringNumber), SLJIT_ARGS2V(P, W));
            sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), back);
        });
    }

    void callListHelper(void *fn, ListOpDescriptor &op, sljit_s32 argTypes) {
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&op));
        callHelper(fn, argTypes);
    }
    void emitCompareJump(const BytecodeInstruction &instr, BytecodeIndex pc);
    void emitNative(const BytecodeInstruction &instr, NativeShape shape);
    bool emitInlineMotion(const BytecodeInstruction &instr, NativeShape shape);
    void emitInlineMod(const BytecodeInstruction &instr);
    ExitDescriptor &makeExit(BytecodeIndex pc, uint32_t result, const AbsState &state);
    uint32_t promoted(BytecodeIndex variable) const { return static_cast<uint32_t>(promotedIndex_[variable]); }
    double constantNumber(BytecodeIndex index) const { return chunk_.constants[index].get<double>(); }
};

void Emitter::resetStateFromAnalysis(BytecodeIndex pc) {
    BytecodeAbstractState s;
    analysis_.stateBefore(pc, s);
    state_.numbers = s.numbers;
    state_.booleans = s.booleans;
    state_.strings.assign(s.strings, StrEntry{});
    state_.values.clear();
    for (BytecodeValueType type : s.values) {
        AbsValue v;
        if (type == BytecodeValueType::Number) v.kind = ValueEntry::Number;
        else if (type == BytecodeValueType::NumericString || type == BytecodeValueType::NumberLike) v.kind = ValueEntry::Like;
        else v.kind = ValueEntry::Opaque;
        state_.values.push_back(v);
    }
}

ExitDescriptor &Emitter::makeExit(BytecodeIndex pc, uint32_t result, const AbsState &state) {
    ExitDescriptor &d = code_.exits.emplace_back();
    d.strings = state.strings;
    d.pc = pc;
    d.result = result;
    d.numbers = state.numbers;
    d.booleans = state.booleans;
    for (const AbsValue &v : state.values) {
        ValueEntry e;
        e.kind = v.kind;
        e.constant = v.constant;
        d.values.push_back(e);
    }
    return d;
}

void Emitter::emitExitAt(BytecodeIndex pc, uint32_t result, const AbsState &state) {
    ExitDescriptor &d = makeExit(pc, result, state);
    emitSpill();
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&d));
    exitJumps_.push_back(sljit_emit_jump(c_, SLJIT_JUMP));
}

void Emitter::emitBackEdgeThenJump(BytecodeIndex target, const AbsState &stateAtTarget) {
    sljit_emit_op2(c_, SLJIT_SUB32, SLJIT_S2, 0, SLJIT_S2, 0, SLJIT_IMM, 1);
    sljit_jump *proceed = sljit_emit_cmp(c_, SLJIT_NOT_EQUAL | SLJIT_32, SLJIT_S2, 0, SLJIT_IMM, 0);
    sljit_emit_op1(c_, SLJIT_MOV32, SLJIT_S2, 0, SLJIT_IMM, kPollInterval);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
    callHelper(reinterpret_cast<void *>(&jitPreempt), SLJIT_ARGS1(W, P));
    sljit_jump *keepGoing = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
    emitExitAt(target, static_cast<uint32_t>(BytecodeJit::Result::Yield), stateAtTarget);
    sljit_label *go = sljit_emit_label(c_);
    sljit_set_label(proceed, go);
    sljit_set_label(keepGoing, go);
    pendUnconditional(target);
}

void Emitter::emitJumpTo(sljit_jump *jump, BytecodeIndex target, const AbsState &stateAtTarget, bool backEdge) {
    if (!backEdge) {
        pendJump(jump, target);
        return;
    }
    addCold([this, jump, target, stateAtTarget]() {
        sljit_set_label(jump, sljit_emit_label(c_));
        emitBackEdgeThenJump(target, stateAtTarget);
    });
}

void Emitter::emitCompareValue(BytecodeCompareKind kind, uint32_t a, uint32_t b) {
    const uint32_t out = booleanSlot(state_.booleans);
    loadF(SLJIT_FR0, a);
    loadF(SLJIT_FR1, b);
    sljit_jump *unordered = sljit_emit_fcmp(c_, SLJIT_UNORDERED, SLJIT_FR0, 0, SLJIT_FR1, 0);

    const sljit_s32 setFlag = kind == BytecodeCompareKind::Equals ? SLJIT_SET_F_EQUAL : kind == BytecodeCompareKind::Less ? SLJIT_SET_F_LESS
                                                                                                                          : SLJIT_SET_F_GREATER;
    const sljit_s32 flag = kind == BytecodeCompareKind::Equals ? SLJIT_F_EQUAL : kind == BytecodeCompareKind::Less ? SLJIT_F_LESS
                                                                                                                   : SLJIT_F_GREATER;
    sljit_emit_fop1(c_, SLJIT_CMP_F64 | setFlag, SLJIT_FR0, 0, SLJIT_FR1, 0);
    sljit_emit_op_flags(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), slotOffset(out), flag);
    sljit_label *done = sljit_emit_label(c_);

    addCold([this, unordered, done, kind, out]() {
        sljit_set_label(unordered, sljit_emit_label(c_));
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, static_cast<sljit_sw>(kind));
        callHelper(reinterpret_cast<void *>(&jitCompareUnordered), SLJIT_ARGS3(W, F64, F64, W));
        storeW(out, SLJIT_R0);
        sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), done);
    });
}

void Emitter::emitCompareJump(const BytecodeInstruction &instr, BytecodeIndex pc) {
    const BytecodeCompareKind kind = static_cast<BytecodeCompareKind>(instr.b);
    const bool jumpIfTrue = instr.op == BytecodeOp::JumpIfNumCompareTrue;
    const uint32_t a = numberSlot(state_.numbers - 2), b = numberSlot(state_.numbers - 1);
    state_.numbers -= 2;
    const AbsState atTarget = state_;
    const bool backEdge = instr.a <= pc;

    loadF(SLJIT_FR0, a);
    loadF(SLJIT_FR1, b);
    sljit_jump *unordered = sljit_emit_fcmp(c_, SLJIT_UNORDERED, SLJIT_FR0, 0, SLJIT_FR1, 0);

    sljit_s32 type;
    switch (kind) {
    case BytecodeCompareKind::Equals:
        type = jumpIfTrue ? SLJIT_ORDERED_EQUAL : SLJIT_ORDERED_NOT_EQUAL;
        break;
    case BytecodeCompareKind::Less:
        type = jumpIfTrue ? SLJIT_ORDERED_LESS : SLJIT_ORDERED_GREATER_EQUAL;
        break;
    default:
        type = jumpIfTrue ? SLJIT_ORDERED_GREATER : SLJIT_ORDERED_LESS_EQUAL;
        break;
    }
    emitJumpTo(sljit_emit_fcmp(c_, type, SLJIT_FR0, 0, SLJIT_FR1, 0), instr.a, atTarget, backEdge);
    sljit_label *fall = sljit_emit_label(c_);

    addCold([this, unordered, fall, kind, jumpIfTrue, atTarget, backEdge, target = instr.a]() {
        sljit_set_label(unordered, sljit_emit_label(c_));
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, static_cast<sljit_sw>(kind));
        callHelper(reinterpret_cast<void *>(&jitCompareUnordered), SLJIT_ARGS3(W, F64, F64, W));
        sljit_jump *no = sljit_emit_cmp(c_, jumpIfTrue ? SLJIT_EQUAL : SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
        if (backEdge) {
            emitBackEdgeThenJump(target, atTarget);
        } else {
            pendUnconditional(target);
        }
        sljit_set_label(no, fall);
    });
}

bool Emitter::emitInlineMotion(const BytecodeInstruction &instr, NativeShape shape) {
    enum Kind { ChangeX,
                ChangeY,
                SetX,
                SetY,
                GoTo } kind;
    const std::string &name = plan_.opcodeName(instr.a);
    if (shape == NativeShape::VoidNumber) {
        if (name == "motion_changexby") kind = ChangeX;
        else if (name == "motion_changeyby") kind = ChangeY;
        else if (name == "motion_setx") kind = SetX;
        else if (name == "motion_sety") kind = SetY;
        else return false;
    } else if (name == "motion_gotoxy") {
        kind = GoTo;
    } else {
        return false;
    }

    const BytecodePositionalFn &fn = link_.positional[instr.a];
    const uint32_t first = shape == NativeShape::VoidNumber ? state_.numbers - 1 : state_.numbers - 2;
    const uint32_t second = state_.numbers - 1;

    code_.doubles.push_back(Scratch::projectWidth * 0.3);
    const double *limitX = &code_.doubles.back();
    code_.doubles.push_back(Scratch::projectHeight * 0.3);
    const double *limitY = &code_.doubles.back();

    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, sprite));
    std::vector<sljit_jump *> slow;
    sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R0), offsetof(Sprite, isStage));
    slow.push_back(sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R1, 0, SLJIT_IMM, 0));
    sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R0), offsetof(Sprite, penData.down));
    slow.push_back(sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R1, 0, SLJIT_IMM, 0));

    const sljit_sw xOffset = offsetof(Sprite, xPosition), yOffset = offsetof(Sprite, yPosition);
    switch (kind) {
    case ChangeX:
        loadF(SLJIT_FR1, first);
        sljit_emit_fop2(c_, SLJIT_ADD_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_R0), xOffset, SLJIT_FR1, 0);
        sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_R0), yOffset);
        break;
    case ChangeY:
        loadF(SLJIT_FR1, first);
        sljit_emit_fop2(c_, SLJIT_ADD_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_R0), yOffset, SLJIT_FR1, 0);
        sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_R0), xOffset);
        break;
    case SetX:
        loadF(SLJIT_FR0, first);
        sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_R0), yOffset);
        break;
    case SetY:
        loadF(SLJIT_FR1, first);
        sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_R0), xOffset);
        break;
    case GoTo:
        loadF(SLJIT_FR0, first);
        loadF(SLJIT_FR1, second);
        break;
    }

    sljit_emit_fop1(c_, SLJIT_ABS_F64, SLJIT_FR2, 0, SLJIT_FR0, 0);
    slow.push_back(sljit_emit_fcmp(c_, SLJIT_UNORDERED_OR_GREATER_EQUAL, SLJIT_FR2, 0, SLJIT_MEM0(), reinterpret_cast<sljit_sw>(limitX)));
    sljit_emit_fop1(c_, SLJIT_ABS_F64, SLJIT_FR2, 0, SLJIT_FR1, 0);
    slow.push_back(sljit_emit_fcmp(c_, SLJIT_UNORDERED_OR_GREATER_EQUAL, SLJIT_FR2, 0, SLJIT_MEM0(), reinterpret_cast<sljit_sw>(limitY)));

    sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), xOffset, SLJIT_FR0, 0);
    sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), yOffset, SLJIT_FR1, 0);
    sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R0), offsetof(Sprite, visible));
    sljit_jump *hidden = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R1, 0, SLJIT_IMM, 0);
    sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_MEM0(), reinterpret_cast<sljit_sw>(&Scratch::forceRedraw), SLJIT_IMM, 1);
    sljit_set_label(hidden, sljit_emit_label(c_));
    sljit_label *done = sljit_emit_label(c_);

    addCold([this, slow, done, shape, first, second, &fn]() {
        const sljit_label *here = sljit_emit_label(c_);
        for (sljit_jump *jump : slow)
            sljit_set_label(jump, const_cast<sljit_label *>(here));
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, sprite));
        loadF(SLJIT_FR0, first);
        if (shape == NativeShape::VoidNumber) {
            callHelper(reinterpret_cast<void *>(fn.n_void), SLJIT_ARGS2V(P, F64));
        } else {
            loadF(SLJIT_FR1, second);
            callHelper(reinterpret_cast<void *>(fn.nn_void), SLJIT_ARGS3V(P, F64, F64));
        }
        sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), done);
    });
    return true;
}

void Emitter::emitInlineMod(const BytecodeInstruction &instr) {
    const BytecodePositionalFn &fn = link_.positional[instr.a];
    const uint32_t a = numberSlot(state_.numbers - 2), b = numberSlot(state_.numbers - 1);
    loadF(SLJIT_FR0, a);
    loadF(SLJIT_FR1, b);

    std::vector<sljit_jump *> slow;
    sljit_emit_fop1(c_, SLJIT_CONV_S32_FROM_F64, SLJIT_R0, 0, SLJIT_FR0, 0);
    sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_S32, SLJIT_FR2, 0, SLJIT_R0, 0);
    slow.push_back(sljit_emit_fcmp(c_, SLJIT_UNORDERED_OR_NOT_EQUAL, SLJIT_FR0, 0, SLJIT_FR2, 0));
    sljit_emit_fop1(c_, SLJIT_CONV_S32_FROM_F64, SLJIT_R1, 0, SLJIT_FR1, 0);
    sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_S32, SLJIT_FR2, 0, SLJIT_R1, 0);
    slow.push_back(sljit_emit_fcmp(c_, SLJIT_UNORDERED_OR_NOT_EQUAL, SLJIT_FR1, 0, SLJIT_FR2, 0));
    slow.push_back(sljit_emit_cmp(c_, SLJIT_EQUAL | SLJIT_32, SLJIT_R1, 0, SLJIT_IMM, 0));
    slow.push_back(sljit_emit_cmp(c_, SLJIT_EQUAL | SLJIT_32, SLJIT_R1, 0, SLJIT_IMM, -1));

    sljit_emit_op1(c_, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_R1, 0);
    sljit_emit_op0(c_, SLJIT_DIVMOD_S32);
    sljit_jump *zero = sljit_emit_cmp(c_, SLJIT_EQUAL | SLJIT_32, SLJIT_R1, 0, SLJIT_IMM, 0);
    sljit_emit_op2(c_, SLJIT_XOR | SLJIT_32, SLJIT_R0, 0, SLJIT_R1, 0, SLJIT_R2, 0);
    sljit_jump *sameSign = sljit_emit_cmp(c_, SLJIT_SIG_GREATER_EQUAL | SLJIT_32, SLJIT_R0, 0, SLJIT_IMM, 0);
    sljit_emit_op2(c_, SLJIT_ADD | SLJIT_32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R2, 0);
    sljit_set_label(sameSign, sljit_emit_label(c_));
    sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_S32, SLJIT_FR0, 0, SLJIT_R1, 0);
    sljit_jump *stored = sljit_emit_jump(c_, SLJIT_JUMP);

    sljit_set_label(zero, sljit_emit_label(c_));
    sljit_emit_fset64(c_, SLJIT_FR2, 0.0);
    sljit_emit_fop2(c_, SLJIT_MUL_F64, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR2, 0);
    sljit_set_label(stored, sljit_emit_label(c_));
    sljit_label *done = sljit_emit_label(c_);
    storeF(a, SLJIT_FR0);

    addCold([this, slow, done, a, b, &fn]() {
        sljit_label *here = sljit_emit_label(c_);
        for (sljit_jump *jump : slow)
            sljit_set_label(jump, here);
        loadF(SLJIT_FR0, a);
        loadF(SLJIT_FR1, b);
        callHelper(reinterpret_cast<void *>(fn.nn_n), SLJIT_ARGS2(F64, F64, F64));
        sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), done);
    });
}

void Emitter::emitNative(const BytecodeInstruction &instr, NativeShape shape) {
    const BytecodePositionalFn &fn = link_.positional[instr.a];
    switch (shape) {
    case NativeShape::Add:
    case NativeShape::Subtract:
    case NativeShape::Multiply:
    case NativeShape::Divide: {
        const uint32_t a = numberSlot(state_.numbers - 2), b = numberSlot(state_.numbers - 1);
        loadF(SLJIT_FR0, a);
        loadF(SLJIT_FR1, b);
        const sljit_s32 op = shape == NativeShape::Add ? SLJIT_ADD_F64 : shape == NativeShape::Subtract ? SLJIT_SUB_F64
                                                                     : shape == NativeShape::Multiply   ? SLJIT_MUL_F64
                                                                                                        : SLJIT_DIV_F64;
        sljit_emit_fop2(c_, op, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR1, 0);
        storeF(a, SLJIT_FR0);
        state_.numbers -= 1;
        break;
    }
    case NativeShape::NumberNumber: {
        if (plan_.opcodeName(instr.a) == "operator_mod") {
            emitInlineMod(instr);
            state_.numbers -= 1;
            break;
        }
        const uint32_t a = numberSlot(state_.numbers - 2), b = numberSlot(state_.numbers - 1);
        loadF(SLJIT_FR0, a);
        loadF(SLJIT_FR1, b);
        callHelper(reinterpret_cast<void *>(fn.nn_n), SLJIT_ARGS2(F64, F64, F64));
        storeF(a, SLJIT_FR0);
        state_.numbers -= 1;
        break;
    }
    case NativeShape::Number: {
        const uint32_t a = numberSlot(state_.numbers - 1);
        loadF(SLJIT_FR0, a);
        callHelper(reinterpret_cast<void *>(fn.n_n), SLJIT_ARGS1(F64, F64));
        storeF(a, SLJIT_FR0);
        break;
    }
    case NativeShape::SpriteNumber: {
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, sprite));
        callHelper(reinterpret_cast<void *>(fn.sprite_n), SLJIT_ARGS1(F64, P));
        storeF(numberSlot(state_.numbers), SLJIT_FR0);
        state_.numbers += 1;
        break;
    }
    case NativeShape::And:
    case NativeShape::Or: {
        const uint32_t a = booleanSlot(state_.booleans - 2), b = booleanSlot(state_.booleans - 1);
        loadW(SLJIT_R0, a);
        loadW(SLJIT_R1, b);
        sljit_emit_op2(c_, shape == NativeShape::And ? SLJIT_AND : SLJIT_OR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
        storeW(a, SLJIT_R0);
        state_.booleans -= 1;
        break;
    }
    case NativeShape::Not: {
        const uint32_t a = booleanSlot(state_.booleans - 1);
        loadW(SLJIT_R0, a);
        sljit_emit_op2(c_, SLJIT_XOR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
        storeW(a, SLJIT_R0);
        break;
    }
    case NativeShape::BoolBool: {
        const uint32_t a = booleanSlot(state_.booleans - 2), b = booleanSlot(state_.booleans - 1);
        loadW(SLJIT_R0, a);
        loadW(SLJIT_R1, b);
        callHelper(reinterpret_cast<void *>(fn.bb_b), SLJIT_ARGS2(32, 32, 32));
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R0, 0, SLJIT_R0, 0);
        storeW(a, SLJIT_R0);
        state_.booleans -= 1;
        break;
    }
    case NativeShape::BoolUnary: {
        const uint32_t a = booleanSlot(state_.booleans - 1);
        loadW(SLJIT_R0, a);
        callHelper(reinterpret_cast<void *>(fn.b_b), SLJIT_ARGS1(32, 32));
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R0, 0, SLJIT_R0, 0);
        storeW(a, SLJIT_R0);
        break;
    }
    case NativeShape::SpriteBoolean: {
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, sprite));
        callHelper(reinterpret_cast<void *>(fn.sprite_b), SLJIT_ARGS1(32, P));
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R0, 0, SLJIT_R0, 0);
        storeW(booleanSlot(state_.booleans), SLJIT_R0);
        state_.booleans += 1;
        break;
    }
    case NativeShape::VoidNone:
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, sprite));
        callHelper(reinterpret_cast<void *>(fn.voidFn), SLJIT_ARGS1V(P));
        break;
    case NativeShape::VoidNumber:
        if (emitInlineMotion(instr, shape)) {
            state_.numbers -= 1;
            break;
        }
        loadF(SLJIT_FR0, numberSlot(state_.numbers - 1));
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, sprite));
        callHelper(reinterpret_cast<void *>(fn.n_void), SLJIT_ARGS2V(P, F64));
        state_.numbers -= 1;
        break;
    case NativeShape::VoidNumberNumber:
        if (emitInlineMotion(instr, shape)) {
            state_.numbers -= 2;
            break;
        }
        loadF(SLJIT_FR0, numberSlot(state_.numbers - 2));
        loadF(SLJIT_FR1, numberSlot(state_.numbers - 1));
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, sprite));
        callHelper(reinterpret_cast<void *>(fn.nn_void), SLJIT_ARGS3V(P, F64, F64));
        state_.numbers -= 2;
        break;
    case NativeShape::VoidValue: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        ValueCallDescriptor &call = code_.valueCalls.emplace_back();
        call.fn = fn.v_void;
        call.itemPosition = depth - 1;
        call.item = describe(state_.values[depth - 1]);
        state_.values.pop_back();
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&call));
        callHelper(reinterpret_cast<void *>(&jitCallValueVoid), SLJIT_ARGS2V(P, P));
        break;
    }
    case NativeShape::StringStringString: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::CallStringString);
        op.fn = reinterpret_cast<void *>(fn.ss_s);
        op.a = state_.strings[n - 2];
        op.aPosition = n - 2;
        op.b = state_.strings[n - 1];
        op.bPosition = n - 1;
        op.outPosition = n - 2;
        state_.strings.pop_back();
        state_.strings.back().kind = StrEntry::Temp;
        callStringOp(op);
        break;
    }
    case NativeShape::StringStringBool: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::CallStringStringBool);
        op.fn = reinterpret_cast<void *>(fn.ss_b);
        op.a = state_.strings[n - 2];
        op.aPosition = n - 2;
        op.b = state_.strings[n - 1];
        op.bPosition = n - 1;
        op.numberOut = booleanSlot(state_.booleans);
        state_.strings.pop_back();
        state_.strings.pop_back();
        state_.booleans++;
        callStringOp(op);
        break;
    }
    case NativeShape::StringNumber: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::CallStringNumber);
        op.fn = reinterpret_cast<void *>(fn.s_n);
        op.a = state_.strings[n - 1];
        op.aPosition = n - 1;
        op.numberOut = numberSlot(state_.numbers);
        state_.strings.pop_back();
        state_.numbers++;
        callStringOp(op);
        break;
    }
    case NativeShape::NumberStringString: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::CallNumberStringString);
        op.fn = reinterpret_cast<void *>(fn.ns_s);
        op.numberIn = numberSlot(state_.numbers - 1);
        op.a = state_.strings[n - 1];
        op.aPosition = n - 1;
        op.outPosition = n - 1;
        state_.numbers--;
        state_.strings.back().kind = StrEntry::Temp;
        callStringOp(op);
        break;
    }
    case NativeShape::SpriteStringNumber: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::CallSpriteStringNumber);
        op.fn = reinterpret_cast<void *>(fn.spriteS_n);
        op.a = state_.strings[n - 1];
        op.aPosition = n - 1;
        op.numberOut = numberSlot(state_.numbers);
        state_.strings.pop_back();
        state_.numbers++;
        callStringOp(op);
        break;
    }
    case NativeShape::SpriteStringBool: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::CallSpriteStringBool);
        op.fn = reinterpret_cast<void *>(fn.spriteS_b);
        op.a = state_.strings[n - 1];
        op.aPosition = n - 1;
        op.numberOut = booleanSlot(state_.booleans);
        state_.strings.pop_back();
        state_.booleans++;
        callStringOp(op);
        break;
    }
    case NativeShape::SpriteString: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::CallSpriteString);
        op.fn = reinterpret_cast<void *>(fn.sprite_s);
        op.outPosition = n;
        StrEntry entry;
        entry.kind = StrEntry::Temp;
        state_.strings.push_back(entry);
        callStringOp(op);
        break;
    }
    case NativeShape::VoidString: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::VoidString);
        op.fn = reinterpret_cast<void *>(fn.s_void);
        op.a = state_.strings[n - 1];
        op.aPosition = n - 1;
        state_.strings.pop_back();
        callStringOp(op);
        break;
    }
    case NativeShape::VoidStringNumber: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::VoidStringNumber);
        op.fn = reinterpret_cast<void *>(fn.sn_void);
        op.a = state_.strings[n - 1];
        op.aPosition = n - 1;
        op.numberIn = numberSlot(state_.numbers - 1);
        state_.strings.pop_back();
        state_.numbers--;
        callStringOp(op);
        break;
    }
    case NativeShape::None:
        break;
    }
}

void Emitter::emitInstruction(BytecodeIndex pc) {
    const BytecodeInstruction &instr = chunk_.instructions[pc];
    switch (instr.op) {
    case BytecodeOp::Nop:
        return;

    case BytecodeOp::PushConstNumber:
        storeImmF(numberSlot(state_.numbers), chunk_.numberConstants[instr.a]);
        state_.numbers++;
        maxNumbers_ = std::max(maxNumbers_, state_.numbers);
        return;
    case BytecodeOp::PushConstBoolean:
        storeImmW(booleanSlot(state_.booleans), chunk_.boolConstants[instr.a] ? 1 : 0);
        state_.booleans++;
        return;
    case BytecodeOp::PopNumber:
        state_.numbers--;
        return;
    case BytecodeOp::PopBoolean:
        state_.booleans--;
        return;
    case BytecodeOp::PopValue:
        state_.values.pop_back();
        return;

    case BytecodeOp::PushVarNumber: {
        ensureVariableNumber(instr.a);
        loadF(SLJIT_FR0, variableSlot(promoted(instr.a)));
        nanToZero(SLJIT_FR0);
        storeF(numberSlot(state_.numbers), SLJIT_FR0);
        state_.numbers++;
        return;
    }
    case BytecodeOp::StoreVarNumber: {
        const uint32_t k = promoted(instr.a);
        loadF(SLJIT_FR0, numberSlot(state_.numbers - 1));
        storeF(variableSlot(k), SLJIT_FR0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 1);
        state_.numbers--;
        return;
    }
    case BytecodeOp::ChangeVarNumber: {
        ensureVariableNumber(instr.a);
        const uint32_t k = promoted(instr.a);
        loadF(SLJIT_FR0, variableSlot(k));
        nanToZero(SLJIT_FR0);
        loadF(SLJIT_FR1, numberSlot(state_.numbers - 1));
        sljit_emit_fop2(c_, SLJIT_ADD_F64, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR1, 0);
        storeF(variableSlot(k), SLJIT_FR0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 1);
        state_.numbers--;
        return;
    }

    case BytecodeOp::PushConstValue: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        storeImmF(valueSlot(depth), chunk_.constants[instr.a].numberRaw());
        AbsValue v;
        v.kind = chunk_.constants[instr.a].isDouble() ? ValueEntry::Number : ValueEntry::Constant;
        v.constant = instr.a;
        state_.values.push_back(v);
        maxValues_ = std::max(maxValues_, depth + 1);
        return;
    }
    case BytecodeOp::ConvertNumberToValue: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        loadF(SLJIT_FR0, numberSlot(state_.numbers - 1));
        storeF(valueSlot(depth), SLJIT_FR0);
        AbsValue v;
        v.kind = ValueEntry::Number;
        state_.values.push_back(v);
        state_.numbers--;
        maxValues_ = std::max(maxValues_, depth + 1);
        return;
    }
    case BytecodeOp::ConvertValueToNumber: {
        const AbsValue top = state_.values.back();
        state_.values.pop_back();
        if (top.kind == ValueEntry::Constant) {
            storeImmF(numberSlot(state_.numbers), constantNumber(top.constant));
        } else {
            loadF(SLJIT_FR0, valueSlot(static_cast<uint32_t>(state_.values.size())));
            nanToZero(SLJIT_FR0);
            storeF(numberSlot(state_.numbers), SLJIT_FR0);
        }
        state_.numbers++;
        maxNumbers_ = std::max(maxNumbers_, state_.numbers);
        return;
    }
    case BytecodeOp::StoreVarValue: {
        const uint32_t k = promoted(instr.a);
        const AbsValue top = state_.values.back();
        state_.values.pop_back();
        const uint32_t position = static_cast<uint32_t>(state_.values.size());
        if (top.kind == ValueEntry::Constant) {
            storeImmF(variableSlot(k), constantNumber(top.constant));
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 5 + static_cast<sljit_sw>(top.constant));
        } else if (top.kind == ValueEntry::Number) {
            loadF(SLJIT_FR0, valueSlot(position));
            storeF(variableSlot(k), SLJIT_FR0);
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 2);
        } else {
            ShadowDescriptor &shadow = code_.shadows.emplace_back();
            shadow.variable = k;
            shadow.entry = describe(top);
            shadow.position = position;
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&shadow));
            callHelper(reinterpret_cast<void *>(&jitStoreShadow), SLJIT_ARGS2V(P, P));
            loadF(SLJIT_FR0, valueSlot(position));
            storeF(variableSlot(k), SLJIT_FR0);
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 3);
        }
        return;
    }

    case BytecodeOp::NumEquals:
    case BytecodeOp::NumLess:
    case BytecodeOp::NumGreater:
        emitCompareValue(instr.op == BytecodeOp::NumEquals ? BytecodeCompareKind::Equals : instr.op == BytecodeOp::NumLess ? BytecodeCompareKind::Less
                                                                                                                           : BytecodeCompareKind::Greater,
                         numberSlot(state_.numbers - 2), numberSlot(state_.numbers - 1));
        state_.numbers -= 2;
        state_.booleans++;
        maxBooleans_ = std::max(maxBooleans_, state_.booleans);
        return;

    case BytecodeOp::ValueEquals:
    case BytecodeOp::ValueLess:
    case BytecodeOp::ValueGreater: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        const BytecodeCompareKind kind = instr.op == BytecodeOp::ValueEquals ? BytecodeCompareKind::Equals : instr.op == BytecodeOp::ValueLess ? BytecodeCompareKind::Less
                                                                                                                                               : BytecodeCompareKind::Greater;
        BytecodeAbstractState s;
        analysis_.stateBefore(pc, s);
        const bool bothNumeric = s.values.size() >= 2 && Planner::numericLike(s.values[s.values.size() - 1]) && Planner::numericLike(s.values[s.values.size() - 2]);
        if (bothNumeric) {
            emitCompareValue(kind, valueSlot(depth - 2), valueSlot(depth - 1));
        } else {
            CompareDescriptor &compare = code_.compares.emplace_back();
            compare.kind = kind;
            compare.a = describe(state_.values[depth - 2]);
            compare.aPosition = depth - 2;
            compare.b = describe(state_.values[depth - 1]);
            compare.bPosition = depth - 1;
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&compare));
            callHelper(reinterpret_cast<void *>(&jitCompareValues), SLJIT_ARGS2(W, P, P));
            storeW(booleanSlot(state_.booleans), SLJIT_R0);
        }
        state_.values.pop_back();
        state_.values.pop_back();
        state_.booleans++;
        maxBooleans_ = std::max(maxBooleans_, state_.booleans);
        return;
    }

    case BytecodeOp::PushArg: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        loadF(SLJIT_FR0, valueSlot(instr.a));
        storeF(valueSlot(depth), SLJIT_FR0);
        AbsValue v;
        v.kind = ValueEntry::Argument;
        v.constant = instr.a;
        state_.values.push_back(v);
        maxValues_ = std::max(maxValues_, depth + 1);
        return;
    }

    case BytecodeOp::CallProcedure:
    case BytecodeOp::CallProcedureValue: {
        const BytecodeProcedure &proc = chunk_.procedures[instr.a];
        CallDescriptor &call = code_.calls.emplace_back();
        call.state = &makeExit(pc, 0, state_);
        call.callPc = pc;
        call.procedure = instr.a;
        call.argCount = instr.b;
        call.wantsValue = instr.op == BytecodeOp::CallProcedureValue;
        call.calleeEntry = static_cast<uint32_t>(plan_.entryOf[proc.entryInstruction]);
        call.leanKinds = state_.strings.empty();
        for (const AbsValue &value : state_.values) {
            if (value.kind != ValueEntry::Number && value.kind != ValueEntry::Constant && value.kind != ValueEntry::Temp && value.kind != ValueEntry::Opaque && value.kind != ValueEntry::Like) call.leanKinds = false;
        }
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&call));
        callHelper(reinterpret_cast<void *>(&jitCall), SLJIT_ARGS2(W, P, P));
        sljit_jump *native = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
        leaveJumps_.push_back(sljit_emit_jump(c_, SLJIT_JUMP));
        sljit_set_label(native, sljit_emit_label(c_));
        pending_.push_back({sljit_emit_jump(c_, SLJIT_JUMP), proc.entryInstruction});
        return;
    }

    case BytecodeOp::Return:
    case BytecodeOp::Halt: {
        ReturnDescriptor &ret = code_.returns.emplace_back();
        ret.state = &makeExit(pc, 0, state_);
        ret.pc = pc;
        ret.isReturn = instr.op == BytecodeOp::Return;
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&ret));
        callHelper(reinterpret_cast<void *>(&jitReturn), SLJIT_ARGS2(W, P, P));
        sljit_jump *leave = sljit_emit_cmp(c_, SLJIT_SIG_LESS, SLJIT_R0, 0, SLJIT_IMM, 0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(code_.entryTable.data()));
        sljit_emit_ijump(c_, SLJIT_JUMP, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), SLJIT_WORD_SHIFT);
        addCold([this, leave]() {
            sljit_set_label(leave, sljit_emit_label(c_));
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1 + static_cast<sljit_sw>(BytecodeJit::Result::Interpret));
            leaveJumps_.push_back(sljit_emit_jump(c_, SLJIT_JUMP));
        });
        return;
    }

    case BytecodeOp::ItemOfList: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        op.indexPosition = depth - 1;
        if (plan_.topValuesAreNumeric(pc, 1)) {
            callListHelper(reinterpret_cast<void *>(&jitListItem), op, SLJIT_ARGS2V(P, P));
        } else {
            op.item = describe(state_.values[depth - 1]);
            op.itemPosition = depth - 1;
            callListHelper(reinterpret_cast<void *>(&jitListItemAny), op, SLJIT_ARGS2V(P, P));
        }
        state_.values.back().kind = ValueEntry::Temp;
        return;
    }
    case BytecodeOp::ReplaceInList:
    case BytecodeOp::InsertInList: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        op.indexPosition = depth - 1;
        op.itemPosition = depth - 2;
        op.item = describe(state_.values[depth - 2]);
        const bool replace = instr.op == BytecodeOp::ReplaceInList;
        void *helper;
        if (plan_.topValuesAreNumeric(pc, 1)) {
            helper = reinterpret_cast<void *>(replace ? &jitListReplace : &jitListInsert);
        } else {
            op.index = describe(state_.values[depth - 1]);
            helper = reinterpret_cast<void *>(replace ? &jitListReplaceAny : &jitListInsertAny);
        }
        state_.values.pop_back();
        state_.values.pop_back();
        callListHelper(helper, op, SLJIT_ARGS2V(P, P));
        return;
    }
    case BytecodeOp::DeleteFromList: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        op.indexPosition = depth - 1;
        const bool numeric = plan_.topValuesAreNumeric(pc, 1);
        if (!numeric) op.index = describe(state_.values[depth - 1]);
        state_.values.pop_back();
        callListHelper(reinterpret_cast<void *>(numeric ? &jitListDelete : &jitListDeleteAny), op, SLJIT_ARGS2V(P, P));
        return;
    }
    case BytecodeOp::AddToList: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        op.itemPosition = depth - 1;
        op.item = describe(state_.values[depth - 1]);
        state_.values.pop_back();
        callListHelper(reinterpret_cast<void *>(&jitListAdd), op, SLJIT_ARGS2V(P, P));
        return;
    }
    case BytecodeOp::DeleteAllOfList: {
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        callListHelper(reinterpret_cast<void *>(&jitListClear), op, SLJIT_ARGS2V(P, P));
        return;
    }
    case BytecodeOp::LengthOfList: {
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        callListHelper(reinterpret_cast<void *>(&jitListLength), op, SLJIT_ARGS2(F64, P, P));
        storeF(numberSlot(state_.numbers), SLJIT_FR0);
        state_.numbers++;
        maxNumbers_ = std::max(maxNumbers_, state_.numbers);
        return;
    }
    case BytecodeOp::ItemNumOfList: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        op.itemPosition = depth - 1;
        op.item = describe(state_.values[depth - 1]);
        state_.values.pop_back();
        callListHelper(reinterpret_cast<void *>(&jitListIndexOf), op, SLJIT_ARGS2(F64, P, P));
        storeF(numberSlot(state_.numbers), SLJIT_FR0);
        state_.numbers++;
        maxNumbers_ = std::max(maxNumbers_, state_.numbers);
        return;
    }
    case BytecodeOp::ListContainsItem: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        op.itemPosition = depth - 1;
        op.item = describe(state_.values[depth - 1]);
        state_.values.pop_back();
        callListHelper(reinterpret_cast<void *>(&jitListContains), op, SLJIT_ARGS2(W, P, P));
        storeW(booleanSlot(state_.booleans), SLJIT_R0);
        state_.booleans++;
        maxBooleans_ = std::max(maxBooleans_, state_.booleans);
        return;
    }

    case BytecodeOp::PushVarValue: {
        ensureVariableNumber(instr.a);
        const uint32_t k = promoted(instr.a);
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        loadF(SLJIT_FR0, variableSlot(k));
        storeF(valueSlot(depth), SLJIT_FR0);
        AbsValue v;
        v.kind = ValueEntry::VarValue;
        v.constant = k;
        state_.values.push_back(v);
        maxValues_ = std::max(maxValues_, depth + 1);
        return;
    }

    case BytecodeOp::JumpIfNumCompareFalse:
    case BytecodeOp::JumpIfNumCompareTrue:
        emitCompareJump(instr, pc);
        return;

    case BytecodeOp::JumpIfFalse:
    case BytecodeOp::JumpIfTrue: {
        loadW(SLJIT_R0, booleanSlot(state_.booleans - 1));
        state_.booleans--;
        const bool backEdge = instr.a <= pc;
        sljit_jump *j = sljit_emit_cmp(c_, instr.op == BytecodeOp::JumpIfTrue ? SLJIT_NOT_EQUAL : SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
        emitJumpTo(j, instr.a, state_, backEdge);
        return;
    }
    case BytecodeOp::Jump:
        if (instr.a <= pc) emitBackEdgeThenJump(instr.a, state_);
        else pendUnconditional(instr.a);
        return;

    case BytecodeOp::RepeatCheck: {
        const uint32_t counter = valueSlot(static_cast<uint32_t>(state_.values.size()) - 1);
        loadF(SLJIT_FR0, counter);
        sljit_emit_fop1(c_, SLJIT_CONV_SW_FROM_F64, SLJIT_R0, 0, SLJIT_FR0, 0);
        sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_SW, SLJIT_FR1, 0, SLJIT_R0, 0);
        sljit_jump *fractional = sljit_emit_fcmp(c_, SLJIT_UNORDERED_OR_NOT_EQUAL, SLJIT_FR0, 0, SLJIT_FR1, 0);
        sljit_emit_fset64(c_, SLJIT_FR1, 1.0);
        sljit_emit_fop2(c_, SLJIT_SUB_F64, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR1, 0);
        sljit_label *counted = sljit_emit_label(c_);
        addCold([this, fractional, counted]() {
            sljit_set_label(fractional, sljit_emit_label(c_));
            callHelper(reinterpret_cast<void *>(&jitRepeatRemaining), SLJIT_ARGS1(F64, F64));
            sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), counted);
        });
        sljit_emit_fset64(c_, SLJIT_FR1, 0.0);
        state_.values.back().kind = ValueEntry::Number;
        AbsState exited = state_;
        exited.values.pop_back();
        sljit_jump *done = sljit_emit_fcmp(c_, SLJIT_ORDERED_LESS, SLJIT_FR0, 0, SLJIT_FR1, 0);
        storeF(counter, SLJIT_FR0);
        emitJumpTo(done, instr.a, exited, instr.a <= pc);
        return;
    }

    case BytecodeOp::PushConstString: {
        StrEntry entry;
        entry.kind = StrEntry::Const;
        entry.constant = instr.a;
        state_.strings.push_back(entry);
        return;
    }
    case BytecodeOp::PushVarString: {
        if (plan_.variableBorrowable(pc)) {
            StrEntry entry;
            entry.kind = StrEntry::Variable;
            entry.constant = promoted(instr.a);
            state_.strings.push_back(entry);
            return;
        }
        StringOp &op = newStringOp(StringOp::PushVariable);
        op.variable = promoted(instr.a);
        op.outPosition = static_cast<uint32_t>(state_.strings.size());
        callStringOp(op);
        StrEntry entry;
        entry.kind = StrEntry::Temp;
        state_.strings.push_back(entry);
        return;
    }
    case BytecodeOp::StoreVarString:
    case BytecodeOp::AppendVarString: {
        StringOp &op = newStringOp(instr.op == BytecodeOp::StoreVarString ? StringOp::StoreVariable : StringOp::AppendVariable);
        op.variable = promoted(instr.a);
        op.aPosition = static_cast<uint32_t>(state_.strings.size()) - 1;
        op.a = state_.strings.back();
        state_.strings.pop_back();
        callStringOp(op);
        return;
    }
    case BytecodeOp::PopString:
        state_.strings.pop_back();
        return;
    case BytecodeOp::StrEquals:
    case BytecodeOp::StrLess:
    case BytecodeOp::StrGreater: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::Compare);
        op.compare = instr.op == BytecodeOp::StrEquals ? BytecodeCompareKind::Equals : instr.op == BytecodeOp::StrLess ? BytecodeCompareKind::Less
                                                                                                                       : BytecodeCompareKind::Greater;
        op.a = state_.strings[n - 2];
        op.aPosition = n - 2;
        op.b = state_.strings[n - 1];
        op.bPosition = n - 1;
        op.numberOut = booleanSlot(state_.booleans);
        state_.strings.pop_back();
        state_.strings.pop_back();
        state_.booleans++;
        maxBooleans_ = std::max(maxBooleans_, state_.booleans);
        callStringOp(op);
        return;
    }
    case BytecodeOp::LengthOfVar: {
        StringOp &op = newStringOp(StringOp::LengthOfVariable);
        op.variable = promoted(instr.a);
        op.numberOut = numberSlot(state_.numbers);
        state_.numbers++;
        maxNumbers_ = std::max(maxNumbers_, state_.numbers);
        callStringOp(op);
        return;
    }
    case BytecodeOp::LetterOfVar: {
        StringOp &op = newStringOp(StringOp::LetterOfVariable);
        op.variable = promoted(instr.a);
        op.numberIn = numberSlot(state_.numbers - 1);
        op.outPosition = static_cast<uint32_t>(state_.strings.size());
        state_.numbers--;
        callStringOp(op);
        StrEntry entry;
        entry.kind = StrEntry::Temp;
        state_.strings.push_back(entry);
        return;
    }
    case BytecodeOp::LengthOf: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        StringOp &op = newStringOp(StringOp::LengthOf);
        op.value = describe(state_.values[depth - 1]);
        op.valuePosition = depth - 1;
        op.numberOut = numberSlot(state_.numbers);
        state_.values.pop_back();
        state_.numbers++;
        maxNumbers_ = std::max(maxNumbers_, state_.numbers);
        callStringOp(op);
        return;
    }
    case BytecodeOp::LetterOf: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        StringOp &op = newStringOp(StringOp::LetterOf);
        op.value = describe(state_.values[depth - 1]);
        op.valuePosition = depth - 1;
        op.numberIn = numberSlot(state_.numbers - 1);
        op.outPosition = static_cast<uint32_t>(state_.strings.size());
        state_.values.pop_back();
        state_.numbers--;
        callStringOp(op);
        StrEntry entry;
        entry.kind = StrEntry::Temp;
        state_.strings.push_back(entry);
        return;
    }
    case BytecodeOp::ConvertValueToString: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        StringOp &op = newStringOp(StringOp::ValueToString);
        op.value = describe(state_.values[depth - 1]);
        op.valuePosition = depth - 1;
        op.outPosition = static_cast<uint32_t>(state_.strings.size());
        state_.values.pop_back();
        callStringOp(op);
        StrEntry entry;
        entry.kind = StrEntry::Temp;
        state_.strings.push_back(entry);
        return;
    }
    case BytecodeOp::ConvertStringToValue: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        StringOp &op = newStringOp(StringOp::StringToValue);
        op.a = state_.strings.back();
        op.aPosition = static_cast<uint32_t>(state_.strings.size()) - 1;
        op.outPosition = depth;
        state_.strings.pop_back();
        callStringOp(op);
        AbsValue v;
        v.kind = ValueEntry::Temp;
        state_.values.push_back(v);
        maxValues_ = std::max(maxValues_, depth + 1);
        return;
    }

    case BytecodeOp::ForEachCheck: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        const uint32_t counter = valueSlot(depth - 1), bound = valueSlot(depth - 2);
        const uint32_t k = promoted(instr.b);
        loadF(SLJIT_FR0, counter);
        nanToZero(SLJIT_FR0);
        loadF(SLJIT_FR1, bound);
        nanToZero(SLJIT_FR1);
        AbsState exited = state_;
        exited.values.pop_back();
        exited.values.pop_back();
        sljit_jump *done = sljit_emit_fcmp(c_, SLJIT_ORDERED_GREATER_EQUAL, SLJIT_FR0, 0, SLJIT_FR1, 0);
        sljit_emit_fset64(c_, SLJIT_FR1, 1.0);
        sljit_emit_fop2(c_, SLJIT_ADD_F64, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR1, 0);
        storeF(counter, SLJIT_FR0);
        storeF(variableSlot(k), SLJIT_FR0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 1);
        state_.values.back().kind = ValueEntry::Number;
        emitJumpTo(done, instr.a, exited, instr.a <= pc);
        return;
    }

    case BytecodeOp::MathOp:
        loadF(SLJIT_FR0, numberSlot(state_.numbers - 1));
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, static_cast<sljit_sw>(instr.b));
        callHelper(reinterpret_cast<void *>(&jitMathOp), SLJIT_ARGS2(F64, F64, W));
        storeF(numberSlot(state_.numbers - 1), SLJIT_FR0);
        return;

    case BytecodeOp::CallNative:
    case BytecodeOp::CallNativeNumber:
    case BytecodeOp::CallNativeString:
    case BytecodeOp::CallNativeBoolean: {
        emitNative(instr, plan_.nativeShape(instr));
        maxNumbers_ = std::max(maxNumbers_, state_.numbers);
        maxBooleans_ = std::max(maxBooleans_, state_.booleans);
        return;
    }

    case BytecodeOp::Yield: {
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, warp));
        sljit_jump *warp = sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
        emitExitAt(pc + 1, static_cast<uint32_t>(BytecodeJit::Result::Yield), state_);
        sljit_set_label(warp, sljit_emit_label(c_));
        return;
    }

    default:
        return;
    }
}

void Emitter::findRegions() {
    const size_t count = chunk_.instructions.size();
    regionOf_.assign(count, -1);
    if (const char *off = std::getenv("SE_JIT_NOPIN"); off != nullptr && std::string(off) != "0") return;

    std::map<BytecodeIndex, BytecodeIndex> tails;
    for (BytecodeIndex pc = 0; pc < count; pc++) {
        const BytecodeInstruction &instr = chunk_.instructions[pc];
        if (plan_.supported[pc] && isJump(instr.op) && instr.a <= pc && instr.a < count) tails[instr.a] = std::max(tails[instr.a], pc);
    }

    for (const auto &[head, tail] : tails) {
        bool ok = true;
        int hotCalls = 0;
        std::map<uint32_t, int> uses;
        std::set<uint32_t> disqualified;
        std::vector<uint32_t> counters;

        for (BytecodeIndex pc = head; pc <= tail && ok; pc++) {
            if (!analysis_.isReachable(pc)) continue;
            if (!plan_.supported[pc]) {
                ok = false;
                break;
            }
            const BytecodeInstruction &instr = chunk_.instructions[pc];
            switch (instr.op) {
            case BytecodeOp::CallProcedure:
            case BytecodeOp::CallProcedureValue:
            case BytecodeOp::Return:
            case BytecodeOp::Halt:
            case BytecodeOp::ForEachCheck:
                ok = false;
                break;
            case BytecodeOp::AddToList:
            case BytecodeOp::DeleteFromList:
            case BytecodeOp::DeleteAllOfList:
            case BytecodeOp::InsertInList:
            case BytecodeOp::ReplaceInList:
            case BytecodeOp::ItemOfList:
            case BytecodeOp::ItemNumOfList:
            case BytecodeOp::ListContainsItem:
            case BytecodeOp::PushVarString:
            case BytecodeOp::StoreVarString:
            case BytecodeOp::AppendVarString:
            case BytecodeOp::StrEquals:
            case BytecodeOp::StrLess:
            case BytecodeOp::StrGreater:
            case BytecodeOp::LengthOfVar:
            case BytecodeOp::LetterOfVar:
            case BytecodeOp::LetterOf:
            case BytecodeOp::LengthOf:
            case BytecodeOp::ConvertStringToValue:
            case BytecodeOp::ConvertValueToString:
            case BytecodeOp::ValueEquals:
            case BytecodeOp::ValueLess:
            case BytecodeOp::ValueGreater:
            case BytecodeOp::MathOp:
                hotCalls++;
                break;
            case BytecodeOp::CallNative:
            case BytecodeOp::CallNativeNumber:
            case BytecodeOp::CallNativeString:
            case BytecodeOp::CallNativeBoolean: {
                const NativeShape shape = plan_.nativeShape(instr);
                const std::string &name = plan_.opcodeName(instr.a);
                const bool inlined = shape == NativeShape::Add || shape == NativeShape::Subtract || shape == NativeShape::Multiply || shape == NativeShape::Divide || shape == NativeShape::And || shape == NativeShape::Or || shape == NativeShape::Not || name == "operator_mod" || name == "motion_changexby" || name == "motion_changeyby" || name == "motion_setx" || name == "motion_sety" || name == "motion_gotoxy";
                if (!inlined) hotCalls++;
                break;
            }
            default:
                break;
            }
            if (!ok) break;
            if (isJump(instr.op) && instr.a <= pc && instr.a != head) ok = false;

            const int64_t variable = plan_.promotedVariable(instr);
            if (variable >= 0) {
                switch (instr.op) {
                case BytecodeOp::PushVarNumber:
                case BytecodeOp::StoreVarNumber:
                case BytecodeOp::ChangeVarNumber:
                case BytecodeOp::PushVarValue:
                case BytecodeOp::StoreVarValue:
                    uses[static_cast<uint32_t>(variable)]++;
                    break;
                default:
                    disqualified.insert(static_cast<uint32_t>(variable));
                    break;
                }
            }
            if (instr.op == BytecodeOp::RepeatCheck) {
                BytecodeAbstractState state;
                if (analysis_.stateBefore(pc, state) && !state.values.empty()) counters.push_back(valueSlot(static_cast<uint32_t>(state.values.size()) - 1));
            }
        }
        const bool logRegions = [] {
            const char *log = std::getenv("SE_JIT_LOG");
            return log != nullptr && std::string(log) == "2";
        }();
        if (!ok || hotCalls > 2) {
            if (logRegions) Log::log("[jit region] " + std::to_string(head) + ".." + std::to_string(tail) + " rejected: " + (ok ? "calls=" + std::to_string(hotCalls) : std::string("shape")));
            continue;
        }

        for (BytecodeIndex pc = 0; pc < count && ok; pc++) {
            if (pc >= head && pc <= tail) continue;
            const BytecodeInstruction &instr = chunk_.instructions[pc];
            if (plan_.supported[pc] && isJump(instr.op) && instr.a > head && instr.a <= tail) ok = false;
        }
        if (!ok) continue;

        Region region;
        region.head = head;
        region.tail = tail;
        for (uint32_t slot : counters) {
            if (region.slots.size() < kMaxPins && std::find(region.slots.begin(), region.slots.end(), slot) == region.slots.end()) region.slots.push_back(slot);
        }
        uint32_t deepest = 0;
        for (BytecodeIndex pc = head; pc <= tail; pc++) {
            BytecodeAbstractState state;
            if (analysis_.isReachable(pc) && analysis_.stateBefore(pc, state)) deepest = std::max(deepest, state.numbers + 1);
        }
        for (uint32_t slot = 0; slot < std::min<uint32_t>(deepest, 3) && region.slots.size() < kMaxPins; slot++)
            region.slots.push_back(numberSlot(slot));
        std::vector<std::pair<int, uint32_t>> ranked;
        for (const auto &[variable, uses_] : uses) {
            if (disqualified.count(variable) || variable >= promotedIndex_.size() || promotedIndex_[variable] < 0 || stringWritten_[variable]) continue;
            ranked.push_back({-uses_, variableSlot(static_cast<uint32_t>(promotedIndex_[variable]))});
        }
        std::sort(ranked.begin(), ranked.end());
        for (const auto &[negativeUses, slot] : ranked) {
            if (region.slots.size() >= kMaxPins) break;
            region.slots.push_back(slot);
        }
        if (logRegions) Log::log("[jit region] " + std::to_string(head) + ".." + std::to_string(tail) + " pins=" + std::to_string(region.slots.size()) + " uses=" + std::to_string(uses.size()) + " disq=" + std::to_string(disqualified.size()) + " calls=" + std::to_string(hotCalls));
        if (region.slots.empty()) continue;

        regions_.push_back(region);
        for (BytecodeIndex pc = head; pc <= tail; pc++)
            regionOf_[pc] = static_cast<int32_t>(regions_.size()) - 1;
    }
}

bool Emitter::emit() {
    const size_t count = chunk_.instructions.size();

    promotedIndex_.assign(chunk_.variableSlots.size(), -1);
    stringWritten_.assign(chunk_.variableSlots.size(), false);
    for (BytecodeIndex pc = 0; pc < count; pc++) {
        if (!plan_.supported[pc]) continue;
        const BytecodeInstruction &instr = chunk_.instructions[pc];
        const int64_t variable = plan_.promotedVariable(instr);
        if (variable < 0 || static_cast<size_t>(variable) >= promotedIndex_.size()) continue;
        if (instr.op == BytecodeOp::StoreVarString || instr.op == BytecodeOp::AppendVarString) stringWritten_[variable] = true;
        if (promotedIndex_[variable] < 0) {
            promotedIndex_[variable] = static_cast<int32_t>(code_.promoted.size());
            code_.promoted.push_back(static_cast<uint32_t>(variable));
        }
    }
    if (code_.promoted.size() > kMaxPromoted) return giveUp("too many variables kept in slots: " + std::to_string(code_.promoted.size()));

    for (BytecodeIndex pc = 0; pc < count; pc++) {
        BytecodeAbstractState s;
        if (!analysis_.stateBefore(pc, s)) continue;
        maxNumbers_ = std::max(maxNumbers_, s.numbers + 2);
        maxBooleans_ = std::max(maxBooleans_, s.booleans + 2);
        maxValues_ = std::max(maxValues_, static_cast<uint32_t>(s.values.size()) + 2);
        maxStrings_ = std::max(maxStrings_, s.strings + 2);
    }
    if (maxStrings_ > kMaxStrs) return giveUp("string stack too deep for the temporaries: " + std::to_string(maxStrings_));
    if (maxValues_ + 2 > kMaxTemps) return giveUp("value stack too deep for the temporaries: " + std::to_string(maxValues_));
    code_.slotBooleans = maxNumbers_;
    code_.slotValues = code_.slotBooleans + maxBooleans_;
    code_.slotVariables = code_.slotValues + maxValues_;
    if (code_.slotVariables + code_.promoted.size() > kMaxSlots) return giveUp("frame too large: " + std::to_string(code_.slotVariables + code_.promoted.size()) + " slots");

    c_ = sljit_create_compiler(nullptr);
    if (c_ == nullptr) return false;

    sljit_emit_enter(c_, 0, SLJIT_ARGS2(W, P, W), 3 | SLJIT_ENTER_FLOAT(kFirstPin + kMaxPins), 3, 0);

    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_S1, 0);
    callHelper(reinterpret_cast<void *>(&jitEnter), SLJIT_ARGS2V(P, W));
    if (poisonPins_) {
        for (size_t i = 0; i < kMaxPins; i++)
            sljit_emit_fset64(c_, SLJIT_FR(kFirstPin + static_cast<int>(i)), -123456.789);
    }
    sljit_emit_op1(c_, SLJIT_MOV32, SLJIT_S2, 0, SLJIT_IMM, kPollInterval);
    code_.entryTable.assign(plan_.entryPcs.size(), nullptr);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(code_.entryTable.data()));
    sljit_emit_ijump(c_, SLJIT_JUMP, SLJIT_MEM2(SLJIT_R1, SLJIT_S1), SLJIT_WORD_SHIFT);

    findRegions();

    labels_.assign(count, nullptr);
    bool havePrevious = false;
    BytecodeIndex previous = 0;
    for (BytecodeIndex pc = 0; pc < count; pc++) {
        const bool compiled = plan_.supported[pc] != 0;
        if (!compiled && !plan_.needsExit[pc]) continue;

        labels_[pc] = sljit_emit_label(c_);

        active_ = regionOf_[pc] >= 0 ? &regions_[regionOf_[pc]] : nullptr;
        if (active_ != nullptr && pc == active_->head) {
            emitReload();
            regions_[regionOf_[pc]].body = sljit_emit_label(c_);
        }

        if (!(havePrevious && previous + 1 == pc && !analysis_.isBlockStart(pc))) resetStateFromAnalysis(pc);
        havePrevious = true;
        previous = pc;

        if (compiled) {
            emitInstruction(pc);
            const BytecodeOp op = chunk_.instructions[pc].op;
            if (op == BytecodeOp::CallProcedure || op == BytecodeOp::CallProcedureValue || op == BytecodeOp::Return || op == BytecodeOp::Halt) havePrevious = false;
            if (active_ != nullptr && pc == active_->tail && op != BytecodeOp::Jump) emitSpill();
        } else {
            emitExitAt(pc, static_cast<uint32_t>(BytecodeJit::Result::Interpret), state_);
            havePrevious = false;
        }
    }

    active_ = nullptr;
    for (size_t i = 0; i < cold_.size(); i++) {
        auto emitCold = cold_[i];
        emitCold();
    }
    active_ = nullptr;

    sljit_label *exitLabel = sljit_emit_label(c_);
    for (sljit_jump *j : exitJumps_)
        sljit_set_label(j, exitLabel);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
    callHelper(reinterpret_cast<void *>(&jitExit), SLJIT_ARGS2(W, P, P));
    sljit_emit_return(c_, SLJIT_MOV, SLJIT_R0, 0);

    sljit_label *leaveLabel = sljit_emit_label(c_);
    for (sljit_jump *j : leaveJumps_)
        sljit_set_label(j, leaveLabel);
    sljit_emit_op2(c_, SLJIT_SUB, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
    callHelper(reinterpret_cast<void *>(&jitLeave), SLJIT_ARGS2(W, P, W));
    sljit_emit_return(c_, SLJIT_MOV, SLJIT_R0, 0);

    for (auto &[jump, target] : pending_) {
        if (target >= count || labels_[target] == nullptr) {
            sljit_free_compiler(c_);
            c_ = nullptr;
            return false;
        }
        sljit_set_label(jump, labels_[target]);
    }

    for (auto &[jump, region] : pendingBody_)
        sljit_set_label(jump, region->body);

    std::vector<sljit_label *> entryLabels(plan_.entryPcs.size(), nullptr);
    for (size_t i = 0; i < plan_.entryPcs.size(); i++) {
        const BytecodeIndex pc = plan_.entryPcs[i];
        if (pc >= count || regionOf_[pc] < 0 || pc == regions_[regionOf_[pc]].head) continue;
        active_ = &regions_[regionOf_[pc]];
        entryLabels[i] = sljit_emit_label(c_);
        emitReload();
        sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), labels_[pc]);
    }
    active_ = nullptr;
    if (pinViolation_) {
        sljit_free_compiler(c_);
        c_ = nullptr;
        return giveUp("a slot kept in a register was accessed behind its back");
    }

    code_.machine = sljit_generate_code(c_, 0, nullptr);
    if (code_.machine == nullptr) {
        sljit_free_compiler(c_);
        c_ = nullptr;
        return false;
    }
    code_.fn = reinterpret_cast<sljit_sw (*)(Context *, sljit_sw)>(code_.machine);
    for (size_t i = 0; i < plan_.entryPcs.size(); i++) {
        code_.entryTable[i] = reinterpret_cast<void *>(sljit_get_label_addr(entryLabels[i] != nullptr ? entryLabels[i] : labels_[plan_.entryPcs[i]]));
    }
    sljit_free_compiler(c_);
    c_ = nullptr;
    return true;
}

} // namespace

namespace {

struct ProgramCache {
    std::unique_ptr<BytecodeProgramAnalysis> program;
    std::vector<BytecodeProgramAnalysis::Input> inputs;
    std::unordered_map<const BytecodeChunk *, BytecodeAnalysis *> byChunk;
};

std::vector<BytecodeProgramAnalysis::Input> currentInputs() {
    std::vector<BytecodeProgramAnalysis::Input> inputs;
    for (Sprite *sprite : Scratch::sprites) {
        if (sprite->bytecode && sprite->bytecodeLink) inputs.push_back({sprite->name, sprite->bytecode.get(), sprite->bytecodeLink.get()});
    }
    return inputs;
}

BytecodeAnalysis *programAnalysisFor(const BytecodeChunk &chunk) {
    static ProgramCache cache;

    const std::vector<BytecodeProgramAnalysis::Input> now = currentInputs();
    bool same = cache.program != nullptr && now.size() == cache.inputs.size();
    for (size_t i = 0; same && i < now.size(); i++) {
        same = now[i].chunk == cache.inputs[i].chunk && now[i].link == cache.inputs[i].link;
    }

    if (!same) {
        BytecodeProgramAnalysis::Environment environment;
        environment.customExtensions = !Scratch::extensions.empty();
        for (const auto &[id, monitor] : Render::monitors) {
            if (monitor.mode == "slider") environment.sliderVariables.insert(id);
        }
        cache.byChunk.clear();
        cache.inputs = now;
        cache.program = std::make_unique<BytecodeProgramAnalysis>(now, std::move(environment));
        cache.program->run();
        for (size_t i = 0; i < now.size(); i++)
            cache.byChunk[now[i].chunk] = cache.program->chunks()[i].get();
    }

    const auto it = cache.byChunk.find(&chunk);
    return it != cache.byChunk.end() ? it->second : nullptr;
}

} // namespace

namespace BytecodeJit {

bool enabled() {
    static const char *env = std::getenv("SE_JIT");
    if (env != nullptr) return std::string(env) != "0";
    return Scratch::useJit;
}

void compile(const BytecodeChunk &chunk, const BytecodeLinkTable &link) {
    if (chunk.jitTried) return;
    chunk.jitTried = true;
    if (chunk.instructions.empty()) return;
    Timer compileTimer;

    std::unique_ptr<BytecodeAnalysis> standalone;
    BytecodeAnalysis *analysisPtr = programAnalysisFor(chunk);
    if (analysisPtr == nullptr) {
        standalone = std::make_unique<BytecodeAnalysis>(chunk, link);
        standalone->run();
        analysisPtr = standalone.get();
    }
    BytecodeAnalysis &analysis = *analysisPtr;
    for (const BytecodeEntryResult &entry : analysis.entries()) {
        if (!entry.ok) return;
    }

    std::vector<BytecodeIndex> roots;
    for (const BytecodeScript &script : chunk.scripts)
        roots.push_back(script.entryInstruction);
    for (const BytecodeProcedure &proc : chunk.procedures)
        roots.push_back(proc.entryInstruction);

    Planner planner(chunk, link, analysis);
    planner.plan(roots);
    if (planner.entryPcs.empty()) return;

    auto code = std::make_shared<BytecodeJitCode>();
    for (BytecodeIndex pc : planner.entryPcs) {
        BytecodeAbstractState s;
        analysis.stateBefore(pc, s);
        EntryDescriptor e;
        e.numbers = s.numbers;
        e.booleans = s.booleans;
        e.valueIsNumber.assign(s.values.size(), 1);
        e.strings = s.strings;
        code->entries.push_back(std::move(e));
    }

    Emitter emitter(chunk, link, analysis, planner, *code);
    if (!emitter.emit()) return;

    for (size_t i = 0; i < code->entries.size(); i++) {
        for (BytecodeIndex variable : planner.entryReads[i]) {
            const auto it = std::find(code->promoted.begin(), code->promoted.end(), variable);
            if (it != code->promoted.end()) code->entries[i].loadVariables.push_back(static_cast<uint32_t>(it - code->promoted.begin()));
        }
    }

    code->entryIdAt = planner.entryOf;
    chunk.jitEntryAt.assign(chunk.instructions.size(), -1);
    bool anyVisible = false;
    for (size_t i = 0; i < planner.entryPcs.size(); i++) {
        if (!planner.visible[i]) continue;
        chunk.jitEntryAt[planner.entryPcs[i]] = static_cast<int32_t>(i);
        anyVisible = true;
    }
    if (!anyVisible) {
        chunk.jitEntryAt.clear();
        return;
    }
    chunk.jit = code;

    if (const char *log = std::getenv("SE_JIT_LOG"); log != nullptr && std::string(log) != "0") {
        size_t compiled = 0, reachable = 0;
        for (size_t pc = 0; pc < chunk.instructions.size(); pc++) {
            if (analysis.isReachable(static_cast<BytecodeIndex>(pc))) reachable++;
            if (planner.supported[pc]) compiled++;
        }
        std::string blockers;
        std::vector<std::pair<uint32_t, uint8_t>> ranked;
        for (const auto &[op, n] : planner.loopBlockers)
            ranked.push_back({n, op});
        std::sort(ranked.rbegin(), ranked.rend());
        for (size_t i = 0; i < ranked.size() && i < 12; i++)
            blockers += " " + std::to_string(ranked[i].second) + ":" + std::to_string(ranked[i].first);
        Log::log("[jit blockers]" + blockers);
        if (std::string(log) == "2") {
            std::map<int, std::string> perProcedure;
            for (const auto &[key, n] : planner.procedureBlockers)
                perProcedure[key.first] += " " + std::to_string(key.second) + ":" + std::to_string(n);
            for (const auto &[procedure, ops] : perProcedure) {
                const std::string name = procedure >= 0 && chunk.procedures[procedure].procCodeIndex < chunk.strings.size() ? chunk.strings[chunk.procedures[procedure].procCodeIndex] : "(script)";
                Log::log("[jit blockers] " + name + ":" + ops);
            }
        }
        Log::log("[jit] compiled " + std::to_string(compiled) + " of " + std::to_string(reachable) + " reachable instructions, " +
                 std::to_string(code->entries.size()) + " entry points, " + std::to_string(code->promoted.size()) + " variables in slots, " +
                 std::to_string(code->exits.size()) + " exits, " + std::to_string(compileTimer.getTimeMsDouble()) + " ms");
    }
}

Result run(const BytecodeChunk &chunk, BytecodeThread &thread, uint32_t entryId, Timer &sliceTimer, bool &sliceTimerStarted) {
    Context ctx;
    ctx.thread = &thread;
    ctx.chunk = &chunk;
    ctx.code = chunk.jit.get();
    ctx.sprite = thread.sprite;
    ctx.timer = &sliceTimer;
    ctx.timerStarted = &sliceTimerStarted;
    const Code &code = *chunk.jit;
    const Result result = static_cast<Result>(code.fn(&ctx, entryId));

    for (uint32_t i = 0; i < ctx.tempHigh; i++)
        gTemps[i] = Value();
    for (uint32_t i = 0; i < ctx.strHigh; i++)
        std::string().swap(gStrs[i]);
    for (uint32_t k = 0; (ctx.stringVariableUsed[0] | ctx.stringVariableUsed[1]) != 0 && k < kMaxPromoted; k++) {
        if (ctx.stringVariableUsed[k >> 6] & (uint64_t(1) << (k & 63))) std::string().swap(gStringVariables[k]);
    }
    for (uint32_t k = 0; (ctx.shadowUsed[0] | ctx.shadowUsed[1]) != 0 && k < kMaxPromoted; k++) {
        if (ctx.shadowUsed[k >> 6] & (uint64_t(1) << (k & 63))) gShadows[k] = Value();
    }
    return result;
}

} // namespace BytecodeJit

#endif
