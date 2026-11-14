#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "blockExecutor.hpp"
#include "bytecode.hpp"
#include "conversions.hpp"
#include "ir/lir_caps.hpp"
#include "jit_runtime.hpp"
#include "jit_types.hpp"
#include "runtime.hpp"
#include "runtime/data/blueprint.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <log.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace BytecodeJitInternal::Runtime {

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

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
const ListLayout &listLayout() {
    static const ListLayout layout = [] {
        ListLayout l;
        const Value::Layout v = Value::layout();
        l.items = static_cast<BytecodeJitLir::Word>(offsetof(List, items));
        l.valueSize = static_cast<BytecodeJitLir::Word>(v.size);
        l.tag = static_cast<BytecodeJitLir::Word>(v.tag);
        l.flags = static_cast<BytecodeJitLir::Word>(v.flags);
        l.number = static_cast<BytecodeJitLir::Word>(v.number);
        l.numberTag = v.numberTag;
        l.stringTag = v.stringTag;
        l.booleanTag = v.booleanTag;

        const Value typed = Value::stringTypedNumber(1.0);
        l.typedFlags = reinterpret_cast<const uint8_t *>(&typed)[l.flags];

        List probe;
        probe.items.reserve(8);
        probe.items.push_back(Value(3.5));
        probe.items.push_back(Value(-1.25));
        probe.items.push_back(Value(true));
        const char *base = reinterpret_cast<const char *>(&probe) + l.items;
        const char *begin = *reinterpret_cast<const char *const *>(base);
        const char *end = *reinterpret_cast<const char *const *>(base + sizeof(void *));
        const char *capacity = *reinterpret_cast<const char *const *>(base + 2 * sizeof(void *));
        const char *second = begin + l.valueSize;
        const char *third = begin + 2 * l.valueSize;
        l.ok = begin == reinterpret_cast<const char *>(probe.items.data()) && end == begin + 3 * l.valueSize && capacity == begin + 8 * l.valueSize &&
               static_cast<uint8_t>(begin[l.tag]) == l.numberTag && *reinterpret_cast<const double *>(begin + l.number) == 3.5 &&
               static_cast<uint8_t>(second[l.tag]) == l.numberTag && *reinterpret_cast<const double *>(second + l.number) == -1.25 &&
               static_cast<uint8_t>(third[l.tag]) == l.booleanTag && *reinterpret_cast<const bool *>(third + l.number) == true && l.typedFlags != 0;
        return l;
    }();
    return layout;
}
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

Value typedNumberValue(double number) { return Value::stringTypedNumber(number); }

const Value *untouchedTextValue(Context *ctx, uint32_t k) {
    const Variable *var = variableAt(*ctx->thread, ctx->code->promoted[k]);
    if (var == nullptr) return nullptr;
    const auto *text = std::get_if<std::shared_ptr<const std::string>>(&var->value);
    if (text == nullptr) return nullptr;
    if (untouchedText[k] != *text) {
        untouchedText[k] = *text;
        untouchedValue[k] = Value(*text);
    }
    return &untouchedValue[k];
}

Value materializeVariableSlow(Context *ctx, uint32_t k) {
    const Code &code = *ctx->code;
    const int64_t kind = ctx->dirty[k];
    if (kind == 0) {
        if (ctx->numericTagged[k] == 1) return typedNumberValue(ctx->slots[code.slotVariables + k]);
        if (const Value *text = untouchedTextValue(ctx, k)) return *text;
        return BlockExecutor::getVariableValueAs<Value>(variableAt(*ctx->thread, code.promoted[k]));
    }
    if (kind == 3) return shadowValues[k];
    if (kind == 4) return Value(stringVariables[k]);
    return ctx->chunk->constants[kind - 5];
}

void LIR_FUNC jitPushColorConst(Context *ctx, BytecodeJitLir::Word index) {
    ctx->thread->colorStack.push_back(ctx->chunk->colorConstants[index]);
}

void LIR_FUNC jitPushColorVariable(Context *ctx, BytecodeJitLir::Word variable) {
    ctx->thread->colorStack.push_back(BlockExecutor::getVariableValueAs<Color>(variableAt(*ctx->thread, static_cast<uint32_t>(variable))));
}

void LIR_FUNC jitPushColorPromoted(Context *ctx, BytecodeJitLir::Word k) {
    ctx->thread->colorStack.push_back(materializeVariable(ctx, static_cast<uint32_t>(k)).asColor());
}

void LIR_FUNC jitCallColorVoid(Context *ctx, void (*fn)(Sprite *, Color)) {
    BytecodeThread &thread = *ctx->thread;
    const Color color = thread.colorStack.back();
    thread.colorStack.pop_back();
    fn(static_cast<Sprite *>(ctx->sprite), color);
}

Value materializeEntrySlow(Context *ctx, const ValueEntry &entry, uint32_t position) {
    switch (entry.kind) {
    case ValueEntry::Boolean: {
        const double d = ctx->slots[ctx->code->slotValues + position];
        return Value(bitsOf(d) != 0);
    }
    case ValueEntry::Argument: {
        const BytecodeThread &thread = *ctx->thread;
        if (ctx->lazyDepth > 0) {
            const NativeFrame &frame = frames[ctx->nativeDepth - 1];
            const ExitDescriptor &callState = *frame.call->state;
            const uint32_t argc = frame.call->argCount;
            if (entry.constant >= argc || callState.values.size() < argc) return Value();
            const uint32_t at = static_cast<uint32_t>(callState.values.size()) - argc + entry.constant;
            const ValueEntry &arg = callState.values[at];
            if (arg.kind == ValueEntry::Number || arg.kind == ValueEntry::Temp || arg.kind == ValueEntry::Argument || arg.kind == ValueEntry::VarValue) {
                const double number = ctx->slots[ctx->code->slotValues + entry.constant];
                if (const uint64_t typedBits = frameTyped(frame); typedBits != 0 && ((typedBits >> entry.constant) & 1) != 0) return typedNumberValue(number);
                return Value(number);
            }
            if (arg.kind == ValueEntry::Constant) return ctx->chunk->constants[arg.constant];
            const size_t index = static_cast<size_t>(frame.stringAndValueBase >> 32) + at;
            return index < thread.valueStack.size() ? thread.valueStack[index] : Value();
        }
        if (thread.callStack.empty()) return Value();
        const BytecodeCallFrame &frame = thread.callStack.back();
        if (frame.stackBase < frame.argCount || entry.constant >= frame.argCount) return Value();
        const size_t index = static_cast<size_t>(frame.stackBase - frame.argCount) + entry.constant;
        return index < thread.valueStack.size() ? thread.valueStack[index] : Value();
    }
    default:
        if (ctx->lazyDepth > 0 && position < frames[ctx->nativeDepth - 1].call->argCount) {
            ValueEntry argument;
            argument.kind = ValueEntry::Argument;
            argument.constant = position;
            return materializeEntry(ctx, argument, position);
        }
        return ctx->thread->valueStack[ctx->valueBase + position];
    }
}

void refreshDepthLimit(Context *ctx) {
    const size_t depth = ctx->thread->callStack.size();
    ctx->depthLimit = depth >= MAX_CALL_DEPTH ? 0 : static_cast<uint32_t>(MAX_CALL_DEPTH - depth);
}

void refreshSave(Context *ctx) {
    ctx->savePtr = saveStack.data();
    ctx->saveCap = static_cast<uint32_t>(saveStack.size());
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
    ctx->stringExcess = entry.strings;

    ctx->valueBase = static_cast<uint32_t>(thread.valueStack.size()) - static_cast<uint32_t>(entry.valueIsNumber.size());
    for (uint32_t i = 0; i < entry.valueIsNumber.size(); i++) {
        if (i < entry.valueKinds.size() && entry.valueKinds[i] == ValueEntry::Boolean) ctx->slots[code.slotValues + i] = thread.valueStack[ctx->valueBase + i].asBoolean() ? 1.0 : 0.0;
        else if (entry.valueIsNumber[i]) ctx->slots[code.slotValues + i] = thread.valueStack[ctx->valueBase + i].numberRaw();
    }

    ctx->warp = thread.withoutScreenRefresh ? 1 : 0;
}

BytecodeJitLir::Word LIR_FUNC jitEnter(Context *ctx, BytecodeJitLir::Word entryId) {
    BytecodeThread &thread = *ctx->thread;
    const Code &code = *ctx->code;
    const EntryDescriptor &entry = code.entries[entryId];

    loadFrame(ctx, entry);

    std::memset(ctx->dirty, 0, code.promoted.size() * sizeof(int64_t));
    std::memset(ctx->plainLoaded, 0, code.promoted.size());
    std::memset(ctx->numericTagged, 0, code.promoted.size());
    for (uint32_t k : entry.loadVariables) {
        const Variable *var = variableAt(thread, code.promoted[k]);
        const double number = rawVariableNumber(var);
        if (number != number) {
            ctx->slots[code.slotVariables + k] = 0.0;
            if (var == nullptr || var->value.index() == 2) continue;
            if (code.strictNaN[k]) {
                thread.jitSuppressOnce = true;
                return 1;
            }
            shadowValues[k] = BlockExecutor::getVariableValueAs<Value>(const_cast<Variable *>(var));
            ctx->shadowUsed[k >> 6] |= uint64_t(1) << (k & 63);
            ctx->dirty[k] = 3;
            continue;
        }
        ctx->slots[code.slotVariables + k] = number;
        if (var != nullptr && var->value.index() == 2) {
            double text;
            if (const Value *value = untouchedTextValue(ctx, k); value != nullptr && value->plainNumber(text) && text == text) {
                ctx->slots[code.slotVariables + k] = text;
                ctx->numericTagged[k] = 2;
            }
        }
        if (var != nullptr && (var->value.index() == 1 || (var->value.index() == 0 && std::get<Value>(var->value).isDouble()))) {
            ctx->numericTagged[k] = 1;
            if (var->value.index() == 1 || std::get<Value>(var->value).isRealNumber()) {
                ctx->dirty[k] = 2;
                ctx->plainLoaded[k] = 1;
                ctx->loadedNumber[k] = number;
            }
        }
    }

    ctx->nativeDepth = 0;
    ctx->saveTop = 0;
    ctx->lazyDepth = 0;
    ctx->threadWarp = &thread.withoutScreenRefresh;
    ctx->pendingRecursive = &thread.pendingRecursiveCallPc;
    refreshDepthLimit(ctx);
    refreshSave(ctx);
    return 0;
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
    ctx->stringExcess = static_cast<uint32_t>(state->strings.size());
    for (uint32_t i = 0; i < state->strings.size(); i++) {
        const StrEntry &entry = state->strings[i];
        if (entry.kind == StrEntry::Const) thread.stringStack[ctx->stringBase + i] = ctx->chunk->stringConstants[entry.constant];
        else if (entry.kind == StrEntry::Temp) thread.stringStack[ctx->stringBase + i] = strs[i];
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
        const double value = ctx->slots[code.slotVariables + k];
        if (ctx->plainLoaded[k] && (kind == 1 || kind == 2) && bitsOf(value) == bitsOf(ctx->loadedNumber[k])) continue;
        Variable *var = resolveOrCreateVariable(thread, code.promoted[k]);
        if (var == nullptr) continue;
        if (kind == 1) var->value = value;
        else if (kind == 2) var->value = Value(value);
        else if (kind == 3) var->value = shadowValues[k];
        else if (kind == 4) var->value = std::make_shared<std::string>(std::move(stringVariables[k]));
        else var->value = ctx->chunk->constants[kind - 5];
    }
}

void materializeLazyFrames(Context *ctx) {
    if (ctx->lazyDepth == 0) return;
    BytecodeThread &thread = *ctx->thread;
    for (uint32_t depth = ctx->nativeDepth - ctx->lazyDepth; depth < ctx->nativeDepth; depth++) {
        NativeFrame &frame = frames[depth];
        const CallDescriptor *call = frame.call;
        const ExitDescriptor &state = *call->state;
        const uint32_t n = state.numbers, b = state.booleans, v = static_cast<uint32_t>(state.values.size());
        const uint32_t numberBase = static_cast<uint32_t>(frame.numberAndBooleanBase), booleanBase = static_cast<uint32_t>(frame.numberAndBooleanBase >> 32);
        const uint32_t valueBase = static_cast<uint32_t>(frame.stringAndValueBase >> 32);
        const double *save = saveStack.data() + (frame.flags >> 32);

        if (thread.numberStack.size() < numberBase + n) thread.numberStack.resize(numberBase + n);
        for (uint32_t i = 0; i < n; i++)
            thread.numberStack[numberBase + i] = save[i];
        if (thread.boolStack.size() < booleanBase + b) thread.boolStack.resize(booleanBase + b);
        for (uint32_t i = 0; i < b; i++)
            thread.boolStack[booleanBase + i] = bitsOf(save[n + i]) != 0;
        if (thread.valueStack.size() < valueBase + v) thread.valueStack.resize(valueBase + v);
        const uint32_t firstArgument = v - call->argCount;
        for (uint32_t i = 0; i < v; i++) {
            const ValueEntry &entry = state.values[i];
            if (entry.kind == ValueEntry::Number || entry.kind == ValueEntry::Temp || entry.kind == ValueEntry::Argument || entry.kind == ValueEntry::VarValue) {
                if (i >= firstArgument && ((frameTyped(frame) >> (i - firstArgument)) & 1) != 0) thread.valueStack[valueBase + i] = Value::stringTypedNumber(save[n + b + i]);
                else thread.valueStack[valueBase + i].setNumber(save[n + b + i]);
            } else if (entry.kind == ValueEntry::Boolean) {
                thread.valueStack[valueBase + i] = Value(bitsOf(save[n + b + i]) != 0);
            } else if (entry.kind == ValueEntry::Constant) thread.valueStack[valueBase + i] = ctx->chunk->constants[entry.constant];
        }

        BytecodeCallFrame &real = thread.callStack.emplace_back();
        real.procedureIndex = call->procedure;
        real.returnInstruction = call->callPc + 1;
        real.stackBase = valueBase + v;
        real.argCount = call->argCount;
        real.wantsValue = call->wantsValue;
        real.previousWarp = (frame.flags & previousWarpFlag) != 0;
        frame.flags &= ~lazyFlag;
    }
    ctx->lazyDepth = 0;
    refreshDepthLimit(ctx);
}

void materializeInlineFrames(Context *ctx, const ExitDescriptor *exit) {
    if (exit->frames.empty()) return;
    BytecodeThread &thread = *ctx->thread;
    bool previousWarp = ctx->warp != 0;
    for (const InlineFrame &inlined : exit->frames) {
        BytecodeCallFrame &real = thread.callStack.emplace_back();
        real.procedureIndex = inlined.procedure;
        real.returnInstruction = inlined.callPc + 1;
        real.stackBase = ctx->valueBase + inlined.valueStart;
        real.argCount = inlined.argCount;
        real.wantsValue = inlined.wantsValue;
        real.previousWarp = previousWarp;
        previousWarp = true;
    }
    thread.withoutScreenRefresh = true;
}

BytecodeJitLir::Word LIR_FUNC jitExit(Context *ctx, const ExitDescriptor *exit) {
    materializeLazyFrames(ctx);
    syncStacks(ctx, exit);
    materializeInlineFrames(ctx, exit);
    commitVariables(ctx);
    ctx->thread->pc = exit->pc;
    return static_cast<BytecodeJitLir::Word>(exit->result);
}

BytecodeJitLir::Word LIR_FUNC jitLeave(Context *ctx, BytecodeJitLir::Word result) {
    commitVariables(ctx);
    return result;
}

int numberKindOf(const Value &value) {
    if (value.isRealNumber()) return 0;
    if (value.isStringTyped() && !value.isString()) return 1;
    return -1;
}

int variableNumberKind(Context *ctx, uint32_t k) {
    const int64_t kind = ctx->dirty[k];
    if (kind == 1 || kind == 2) return 0;
    if (kind >= 5) return numberKindOf(ctx->chunk->constants[kind - 5]);
    if (kind == 3) return shadowValues[k].numberRaw() != shadowValues[k].numberRaw() ? -1 : numberKindOf(shadowValues[k]);
    if (kind == 0) {
        const Variable *var = variableAt(*ctx->thread, ctx->code->promoted[k]);
        if (var == nullptr) return -1;
        if (std::holds_alternative<double>(var->value)) return 0;
        if (const Value *value = std::get_if<Value>(&var->value)) return numberKindOf(*value);
        return -1;
    }
    return -1;
}

int argumentNumberKind(Context *ctx, uint32_t index) {
    const BytecodeThread &thread = *ctx->thread;
    if (ctx->lazyDepth > 0) {
        const NativeFrame &frame = frames[ctx->nativeDepth - 1];
        const ExitDescriptor &callState = *frame.call->state;
        const uint32_t argc = frame.call->argCount;
        if (index >= argc || callState.values.size() < argc) return -1;
        const ValueEntry &arg = callState.values[callState.values.size() - argc + index];
        if (arg.kind == ValueEntry::Number) return 0;
        if (arg.kind == ValueEntry::Temp || arg.kind == ValueEntry::Argument || arg.kind == ValueEntry::VarValue) return static_cast<int>((frameTyped(frame) >> index) & 1);
        if (arg.kind == ValueEntry::Constant) return numberKindOf(ctx->chunk->constants[arg.constant]);
        return -1;
    }
    if (thread.callStack.empty()) return -1;
    const BytecodeCallFrame &frame = thread.callStack.back();
    if (frame.stackBase < frame.argCount || index >= frame.argCount) return -1;
    const size_t at = static_cast<size_t>(frame.stackBase - frame.argCount) + index;
    return at < thread.valueStack.size() ? numberKindOf(thread.valueStack[at]) : -1;
}

bool lazyCall(Context *ctx, const CallDescriptor *call) {
    BytecodeThread &thread = *ctx->thread;
    if (!(thread.withoutScreenRefresh || call->procWarp)) return false;
    if (thread.callStack.size() + ctx->lazyDepth >= MAX_CALL_DEPTH) return false;

    const Code &code = *ctx->code;
    const uint32_t argc = call->argCount;
    const uint32_t n = call->numbers, b = call->booleans, v = call->values;
    if (call->leanCallee < 0) {
        const ExitDescriptor *state = call->state;
        const EntryDescriptor &entry = code.entries[call->calleeEntry];
        bool lean = argc <= v && entry.numbers == 0 && entry.booleans == 0 && entry.strings == 0 && entry.valueIsNumber.size() == argc;
        for (uint8_t isNumber : entry.valueIsNumber)
            lean = lean && isNumber;
        const int32_t continuation = call->callPc + 1 < code.entryIdAt.size() ? code.entryIdAt[call->callPc + 1] : -1;
        if (lean && continuation >= 0) {
            const EntryDescriptor &after = code.entries[continuation];
            lean = after.numbers == n && after.booleans == b && after.strings == state->strings.size() && after.valueKinds.size() == (v - argc) + (call->wantsValue ? 1 : 0);
            for (uint32_t i = 0; lean && i < v - argc; i++) {
                const ValueEntry::Kind was = state->values[i].kind, will = static_cast<ValueEntry::Kind>(after.valueKinds[i]);
                const bool real = will == ValueEntry::Opaque || will == ValueEntry::Like;
                lean = (was == ValueEntry::Opaque || was == ValueEntry::Like) ? real : (was == ValueEntry::Number && will == ValueEntry::Number);
            }
            if (lean) {
                call->continuation = continuation;
                call->resultNumber = !call->wantsValue || after.valueKinds[v - argc] == ValueEntry::Number;
            }
        } else {
            lean = false;
        }
        call->leanCallee = lean ? 1 : 0;
        call->fast = lean && call->resultNumber && call->nativeChecks ? 1 : 0;
    }
    if (call->leanCallee == 0) return false;
    if (thread.stringStack.size() != ctx->stringBase) return false;

    const uint32_t kept = v - argc;
    uint64_t typed = 0;
    if (!call->checks.empty()) {
        const auto &values = thread.valueStack;
        const uint32_t virtualEntries = ctx->lazyDepth > 0 ? frames[ctx->nativeDepth - 1].call->argCount : 0;
        for (const CallCheck &check : call->checks) {
            const uint32_t i = check.index;
            int how = 0;
            switch (check.kind) {
            case ValueEntry::Opaque:
            case ValueEntry::Like:
                if (i >= virtualEntries && ctx->valueBase + i >= values.size()) return false;
                break;
            case ValueEntry::Temp:
                how = tempLazy[i] == 1 ? 0 : tempLazy[i] == 2 ? 1
                                                              : numberKindOf(temps[i]);
                break;
            case ValueEntry::Argument:
                how = argumentNumberKind(ctx, check.constant);
                break;
            default:
                how = variableNumberKind(ctx, check.constant);
                break;
            }
            if (how < 0) return false;
            if (how == 1) {
                if (i < kept) return false;
                typed |= uint64_t(1) << (i - kept);
            }
        }
    }

    const size_t need = static_cast<size_t>(n) + b + v;
    if (ctx->saveTop + need > saveStack.size()) {
        saveStack.resize(std::max<size_t>(saveStack.size() * 2, ctx->saveTop + need + 256));
        refreshSave(ctx);
    }
    double *valueSlots = ctx->slots + code.slotValues;
    double *save = saveStack.data() + ctx->saveTop;
    for (uint32_t i = 0; i < n; i++)
        save[i] = ctx->slots[i];
    save += n;
    for (uint32_t i = 0; i < b; i++)
        save[i] = ctx->slots[code.slotBooleans + i];
    save += b;
    for (uint32_t i = 0; i < v; i++)
        save[i] = valueSlots[i];

    NativeFrame &frame = frames[ctx->nativeDepth];
    frame.call = call;
    frame.numberAndBooleanBase = static_cast<uint64_t>(ctx->numberBase) | (static_cast<uint64_t>(ctx->booleanBase) << 32);
    frame.stringAndValueBase = static_cast<uint64_t>(ctx->stringBase) | (static_cast<uint64_t>(ctx->valueBase) << 32);
    frame.flags = lazyFlag | (thread.withoutScreenRefresh ? previousWarpFlag : 0) | (typed << 2) | (static_cast<uint64_t>(ctx->saveTop) << 32);
    ctx->saveTop += static_cast<uint32_t>(need);
    ctx->nativeDepth++;
    ctx->lazyDepth++;

    thread.pendingRecursiveCallPc = invalidBytecodeIndex;
    thread.withoutScreenRefresh = thread.withoutScreenRefresh || call->procWarp;
    ctx->numberBase += n;
    ctx->booleanBase += b;
    ctx->valueBase += kept;
    for (uint32_t i = 0; i < argc; i++)
        valueSlots[i] = valueSlots[kept + i];
    ctx->warp = thread.withoutScreenRefresh ? 1 : 0;
    return true;
}

BytecodeJitLir::Word LIR_FUNC jitCall(Context *ctx, const CallDescriptor *call) {
    BytecodeThread &thread = *ctx->thread;
    const Code &code = *ctx->code;

    if (call->leanKinds && lazyCall(ctx, call)) return 0;
    materializeLazyFrames(ctx);
    frames[ctx->nativeDepth].flags = 0;
    syncStacks(ctx, call->state);

    if (thread.callStack.size() >= MAX_CALL_DEPTH) {
        thread.pc = call->callPc + 1;
        return 1 + static_cast<BytecodeJitLir::Word>(BytecodeJit::Result::Interpret);
    }

    const BytecodeProcedure &proc = ctx->chunk->procedures[call->procedure];

    if (!thread.withoutScreenRefresh && !proc.runWithoutScreenRefresh && thread.pendingRecursiveCallPc != call->callPc) {
        if (bytecodeIsRecursiveCall(*ctx->chunk, thread.callStack, call->procedure, call->callPc)) {
            thread.pendingRecursiveCallPc = call->callPc;
            thread.pc = call->callPc;
            return 1 + static_cast<BytecodeJitLir::Word>(BytecodeJit::Result::Yield);
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
    refreshDepthLimit(ctx);

    thread.withoutScreenRefresh = thread.withoutScreenRefresh || proc.runWithoutScreenRefresh;
    thread.pc = proc.entryInstruction;

    ctx->nativeDepth++;
    loadFrame(ctx, code.entries[call->calleeEntry]);
    return 0;
}

BytecodeJitLir::Word lazyReturn(Context *ctx, const ReturnDescriptor *ret) {
    BytecodeThread &thread = *ctx->thread;
    const Code &code = *ctx->code;
    const ExitDescriptor *state = ret->state;
    NativeFrame &frame = frames[ctx->nativeDepth - 1];
    const CallDescriptor *call = frame.call;
    const uint32_t n = call->numbers, b = call->booleans, v = call->values;
    const uint32_t argc = call->argCount, kept = v - argc;
    const bool wantsValue = call->wantsValue;

    double number = 0.0;
    Value built;
    bool haveValue = false;
    bool plainNumber = false;
    if (wantsValue && ret->isReturn && !state->values.empty()) {
        const ValueEntry &top = state->values.back();
        const uint32_t position = static_cast<uint32_t>(state->values.size()) - 1;
        if (top.kind == ValueEntry::Number) {
            number = ctx->slots[code.slotValues + position];
            plainNumber = true;
        } else {
            built = materializeEntry(ctx, top, position);
            number = built.numberRaw();
            haveValue = true;
        }
    }

    const bool previousWarp = (frame.flags & previousWarpFlag) != 0;
    const uint32_t saveStart = static_cast<uint32_t>(frame.flags >> 32);
    ctx->nativeDepth--;
    ctx->lazyDepth--;
    ctx->saveTop = saveStart;
    thread.withoutScreenRefresh = previousWarp;

    const double *src = saveStack.data() + saveStart;
    for (uint32_t i = 0; i < n; i++)
        ctx->slots[i] = src[i];
    src += n;
    for (uint32_t i = 0; i < b; i++)
        ctx->slots[code.slotBooleans + i] = src[i];
    src += b;
    double *valueSlots = ctx->slots + code.slotValues;
    for (uint32_t i = 0; i < kept; i++)
        valueSlots[i] = src[i];
    ctx->numberBase = static_cast<uint32_t>(frame.numberAndBooleanBase);
    ctx->booleanBase = static_cast<uint32_t>(frame.numberAndBooleanBase >> 32);
    ctx->stringBase = static_cast<uint32_t>(frame.stringAndValueBase);
    ctx->valueBase = static_cast<uint32_t>(frame.stringAndValueBase >> 32);
    ctx->warp = previousWarp ? 1 : 0;

    const int32_t entryId = call->continuation;
    if (wantsValue) {
        valueSlots[kept] = number;
        if (!call->resultNumber) {
            auto &values = thread.valueStack;
            const size_t index = static_cast<size_t>(ctx->valueBase) + kept;
            while (values.size() > index)
                values.pop_back();
            while (values.size() < index)
                values.emplace_back();
            if (haveValue) values.push_back(std::move(built));
            else if (plainNumber) values.emplace_back().setNumber(number);
            else values.emplace_back();
        }
    }
    return entryId;
}

BytecodeJitLir::Word LIR_FUNC jitReturn(Context *ctx, const ReturnDescriptor *ret) {
    BytecodeThread &thread = *ctx->thread;
    const Code &code = *ctx->code;

    if (ctx->lazyDepth > 0) return lazyReturn(ctx, ret);

    syncStacks(ctx, ret->state);

    if (thread.callStack.empty()) {
        thread.pc = ret->pc;
        return -1;
    }

    const BytecodeCallFrame frame = thread.callStack.back();
    thread.callStack.pop_back();
    refreshDepthLimit(ctx);

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

void LIR_FUNC jitCallValueVoid(Context *ctx, const ValueCallDescriptor *call) {
    const Value value = materializeEntry(ctx, call->item, call->itemPosition);
    call->fn(static_cast<Sprite *>(ctx->sprite), value);
}

BytecodeJitLir::Word LIR_FUNC jitCallValueBool(Context *ctx, const ValueCallDescriptor *call) {
    Value scratch;
    return call->fnBool(peekEntry(ctx, call->item, call->itemPosition, scratch)) ? 1 : 0;
}

void LIR_FUNC jitConvertValueToColor(Context *ctx, const ValueCallDescriptor *call) {
    Value scratch;
    ctx->thread->colorStack.push_back(peekEntry(ctx, call->item, call->itemPosition, scratch).asColor());
}

BytecodeJitLir::Word LIR_FUNC jitConvertValueToBoolean(Context *ctx, const ValueCallDescriptor *call) {
    Value scratch;
    return peekEntry(ctx, call->item, call->itemPosition, scratch).get<bool>() ? 1 : 0;
}

double LIR_FUNC jitRandomValues(Context *ctx, const ValueCallDescriptor *call) {
    Value scratchFrom, scratchTo;
    const Value &fromValue = peekEntry(ctx, call->item, call->itemPosition, scratchFrom);
    const Value &toValue = peekEntry(ctx, call->second, call->secondPosition, scratchTo);
    const double a = fromValue.get<double>();
    const double b = toValue.get<double>();
    if (a == b) return a;
    const double from = std::min(a, b);
    const double to = std::max(a, b);
    if (fromValue.isScratchInt() && toValue.isScratchInt()) return from + static_cast<double>(fastRandomBelow(static_cast<uint64_t>(to + 1 - from)));
    return from + fastRandomDouble() * (to - from);
}

void LIR_FUNC jitCostumeNumberName(Context *ctx, BytecodeJitLir::Word position, BytecodeJitLir::Word byNumber) {
    const Sprite *sprite = static_cast<const Sprite *>(ctx->sprite);
    Value result = byNumber == 0 ? Value(sprite->costumes[sprite->currentCostume].name) : Value(sprite->currentCostume + 1);
    ctx->slots[ctx->code->slotValues + position] = result.numberRaw();
    temps[position] = std::move(result);
    tempLazy[position] = 0;
    ctx->tempHigh = std::max(ctx->tempHigh, static_cast<uint32_t>(position) + 1);
}

double LIR_FUNC jitRepeatRound(double counter) {
    if (std::isnan(counter)) return 0.0;
    return std::round(counter);
}

double LIR_FUNC jitRepeatRemaining(double counter) {
    if (std::isnan(counter)) counter = 0.0;
    return std::round(counter) - 1.0;
}

BytecodeJitLir::Word LIR_FUNC jitCompareUnordered(double a, double b, BytecodeJitLir::Word kind) {
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

double LIR_FUNC jitMathOp(double value, BytecodeJitLir::Word op) {
    return bytecodeMathOpApply(static_cast<BytecodeMathOp>(op), value);
}

double LIR_FUNC jitRandomInt(double a, double b) {
    if (a == b) return a;
    const double from = std::min(a, b);
    const double to = std::max(a, b);
    return from + static_cast<double>(fastRandomBelow(static_cast<uint64_t>(to + 1.0 - from)));
}

BytecodeJitLir::Word LIR_FUNC jitPreempt(Context *ctx, BytecodeJitLir::Word target) {
    static const long hotPolls = BytecodeJit::tierUpPolls();
    bool switchCode = false;
    if (hotPolls > 0) BytecodeJit::installFinished();
    if (hotPolls > 0 && ctx->chunk != nullptr && ctx->code == ctx->chunk->jit.get() && target >= 0 && static_cast<size_t>(target) < ctx->chunk->instructions.size()) {
        const BytecodeIndex head = static_cast<BytecodeIndex>(target);
        const auto ready = [&] { return head < ctx->chunk->jitEntryAt.size() && (ctx->chunk->jitEntryAt[head] >> 24) != 0; };
        if (ready()) {
            switchCode = true;
        } else {
            const Code &baseline = *ctx->code;
            if (baseline.loopHeat.size() != ctx->chunk->instructions.size()) {
                baseline.loopHeat.assign(ctx->chunk->instructions.size(), 0);
                baseline.loopTried.assign(ctx->chunk->instructions.size(), 0);
            }
            if (baseline.loopHeat[head] < 0xffff) baseline.loopHeat[head]++;
            if (baseline.loopHeat[head] >= hotPolls && !baseline.loopTried[head] && ctx->thread->link != nullptr) {
                baseline.loopTried[head] = 1;
                BytecodeJit::requestTierUp(*ctx->chunk, *ctx->thread->link, head);
                switchCode = ready();
            }
        }
    }
    static const bool checkNaN = std::getenv("SE_JIT_CHECKNAN") != nullptr;
    if (checkNaN) {
        for (uint32_t k = 0; k < ctx->code->promoted.size(); k++) {
            if (ctx->dirty[k] == 0) continue;
            const double number = ctx->slots[ctx->code->slotVariables + k];
            if (number != number) {
                Log::logCritical("JIT: a promoted variable's slot holds NaN (variable " + std::to_string(k) + ")", true);
                std::abort();
            }
        }
    }
    if (const long every = forcedYieldEvery(); every > 0) {
        static long polls = 0;
        if (++polls % every == 0) return 1;
    }
    if (Scratch::warpTimer && ctx->thread->withoutScreenRefresh) {
        if (!*ctx->timerStarted) {
            ctx->timer->start();
            *ctx->timerStarted = true;
        } else if (ctx->timer->getTimeMs() > 500) {
            return 1;
        }
    }
    return switchCode ? 2 : 0;
}

} // namespace BytecodeJitInternal::Runtime

#endif
