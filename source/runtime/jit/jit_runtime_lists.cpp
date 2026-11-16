#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "bytecode.hpp"
#include "ir/lir_caps.hpp"
#include "jit_runtime.hpp"
#include "jit_types.hpp"
#include "runtime/data/blueprint.hpp"
#include "value.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

namespace BytecodeJitInternal::Runtime {

void LIR_FUNC jitListItem(Context *ctx, const ListOpDescriptor *op) {
    const List *list = listAt(*ctx->thread, op->list);
    Value item;
    if (list != nullptr && !list->items.empty()) {
        const size_t position = listPosition(ctx->slots[ctx->code->slotValues + op->indexPosition], list->items.size());
        if (position < list->items.size()) item = list->items[position];
    }
    ctx->slots[ctx->code->slotValues + op->indexPosition] = item.numberRaw();
    temps[op->indexPosition] = std::move(item);
    tempLazy[op->indexPosition] = 0;
    ctx->tempHigh = std::max(ctx->tempHigh, op->indexPosition + 1);
}

void LIR_FUNC jitListReplaceAny(Context *ctx, const ListOpDescriptor *op) {
    List *list = listAt(*ctx->thread, op->list);
    const Value index = materializeEntry(ctx, op->index, op->indexPosition);
    const Value item = materializeEntry(ctx, op->item, op->itemPosition);
    if (list != nullptr) bytecodeListReplace(*list, index, item);
}

void LIR_FUNC jitListInsertAny(Context *ctx, const ListOpDescriptor *op) {
    List *list = listAt(*ctx->thread, op->list);
    const Value index = materializeEntry(ctx, op->index, op->indexPosition);
    Value item = materializeEntry(ctx, op->item, op->itemPosition);
    if (list != nullptr) bytecodeListInsert(*list, index, std::move(item));
}

void LIR_FUNC jitListDeleteAny(Context *ctx, const ListOpDescriptor *op) {
    List *list = listAt(*ctx->thread, op->list);
    const Value index = materializeEntry(ctx, op->index, op->indexPosition);
    if (list != nullptr) bytecodeListDelete(*list, index);
}

void LIR_FUNC jitListItemAny(Context *ctx, const ListOpDescriptor *op) {
    const List *list = listAt(*ctx->thread, op->list);
    const Value index = materializeEntry(ctx, op->item, op->itemPosition);
    Value item;
    if (list != nullptr && !list->items.empty()) {
        const size_t position = bytecodeResolveListIndex(index, list->items.size());
        if (position < list->items.size()) item = list->items[position];
    }
    ctx->slots[ctx->code->slotValues + op->indexPosition] = item.numberRaw();
    temps[op->indexPosition] = std::move(item);
    tempLazy[op->indexPosition] = 0;
    ctx->tempHigh = std::max(ctx->tempHigh, op->indexPosition + 1);
}

void LIR_FUNC jitStoreShadow(Context *ctx, const ShadowDescriptor *shadow) {
    shadowValues[shadow->variable] = materializeEntry(ctx, shadow->entry, shadow->position);
    ctx->numericTagged[shadow->variable] = shadowValues[shadow->variable].isDouble() && !shadowValues[shadow->variable].isNaN() ? 1 : 0;
    ctx->shadowUsed[shadow->variable >> 6] |= uint64_t(1) << (shadow->variable & 63);
}

void LIR_FUNC jitStoreBoolean(Context *ctx, BytecodeJitLir::Word k, BytecodeJitLir::Word value) {
    ctx->slots[ctx->code->slotVariables + k] = value != 0 ? 1.0 : 0.0;
    ctx->numericTagged[k] = 0;
    shadowValues[k] = Value(value != 0);
    ctx->shadowUsed[k >> 6] |= uint64_t(1) << (k & 63);
    ctx->dirty[k] = 3;
}

void LIR_FUNC jitSetNaN(Context *ctx, BytecodeJitLir::Word k) {
    ctx->slots[ctx->code->slotVariables + k] = 0.0;
    ctx->numericTagged[k] = 0;
    shadowValues[k] = Value(std::numeric_limits<double>::quiet_NaN());
    ctx->shadowUsed[k >> 6] |= uint64_t(1) << (k & 63);
    ctx->dirty[k] = 3;
}

static const Value *entryValue(Context *ctx, const ValueEntry &entry, uint32_t position) {
    switch (entry.kind) {
    case ValueEntry::Constant:
        return &ctx->chunk->constants[entry.constant];
    case ValueEntry::Temp:
        return tempLazy[position] == 0 ? &temps[position] : nullptr;
    case ValueEntry::VarValue: {
        const uint32_t k = entry.constant;
        const int64_t kind = ctx->dirty[k];
        if (kind == 3) return &shadowValues[k];
        if (kind >= 5) return &ctx->chunk->constants[kind - 5];
        if (kind != 0) return nullptr;
        const Variable *var = variableAt(*ctx->thread, ctx->code->promoted[k]);
        return var != nullptr ? std::get_if<Value>(&var->value) : nullptr;
    }
    case ValueEntry::Argument: {
        const BytecodeThread &thread = *ctx->thread;
        size_t index;
        if (ctx->lazyDepth > 0) {
            const NativeFrame &frame = frames[ctx->nativeDepth - 1];
            const ExitDescriptor &callState = *frame.call->state;
            const uint32_t argc = frame.call->argCount;
            if (entry.constant >= argc || callState.values.size() < argc) return nullptr;
            const uint32_t at = static_cast<uint32_t>(callState.values.size()) - argc + entry.constant;
            const ValueEntry &arg = callState.values[at];
            if (arg.kind == ValueEntry::Constant) return &ctx->chunk->constants[arg.constant];
            if (arg.kind == ValueEntry::Number || arg.kind == ValueEntry::Temp || arg.kind == ValueEntry::Argument || arg.kind == ValueEntry::VarValue) return nullptr;
            index = static_cast<size_t>(frame.stringAndValueBase >> 32) + at;
        } else {
            if (thread.callStack.empty()) return nullptr;
            const BytecodeCallFrame &frame = thread.callStack.back();
            if (frame.stackBase < frame.argCount || entry.constant >= frame.argCount) return nullptr;
            index = static_cast<size_t>(frame.stackBase - frame.argCount) + entry.constant;
        }
        return index < thread.valueStack.size() ? &thread.valueStack[index] : nullptr;
    }
    default:
        return nullptr;
    }
}

static bool entryNumber(Context *ctx, const ValueEntry &entry, uint32_t position, double &out) {
    switch (entry.kind) {
    case ValueEntry::Number:
        out = ctx->slots[ctx->code->slotValues + position];
        return true;
    case ValueEntry::VarValue: {
        const uint32_t k = entry.constant;
        const int64_t kind = ctx->dirty[k];
        if (kind == 1 || kind == 2) {
            out = ctx->slots[ctx->code->slotVariables + k];
            return true;
        }
        if ((kind == 0 || kind == 3) && ctx->numericTagged[k]) {
            out = ctx->slots[ctx->code->slotVariables + k];
            return true;
        }
        break;
    }
    case ValueEntry::Argument:
        if (ctx->lazyDepth > 0) {
            const NativeFrame &frame = frames[ctx->nativeDepth - 1];
            const ExitDescriptor &callState = *frame.call->state;
            const uint32_t argc = frame.call->argCount;
            if (entry.constant < argc && callState.values.size() >= argc) {
                const ValueEntry::Kind kind = callState.values[callState.values.size() - argc + entry.constant].kind;
                if (kind == ValueEntry::Number || kind == ValueEntry::Temp || kind == ValueEntry::Argument || kind == ValueEntry::VarValue) {
                    out = ctx->slots[ctx->code->slotValues + entry.constant];
                    return true;
                }
            }
        }
        break;
    case ValueEntry::Temp:
        if (tempLazy[position] == 1 || tempLazy[position] == 2) {
            out = ctx->slots[ctx->code->slotValues + position];
            return true;
        }
        break;
    default:
        break;
    }
    if (const Value *value = entryValue(ctx, entry, position)) return value->plainNumber(out);
    if (entry.kind == ValueEntry::VarValue && ctx->dirty[entry.constant] == 0) {
        const Variable *var = variableAt(*ctx->thread, ctx->code->promoted[entry.constant]);
        if (var != nullptr) {
            if (const double *number = std::get_if<double>(&var->value)) {
                out = *number;
                return true;
            }
        }
    }
    return false;
}

static const Value &compareOperand(Context *ctx, const ValueEntry &entry, uint32_t position, Value &scratch) {
    if (const Value *stored = entryValue(ctx, entry, position)) return *stored;
    if (entry.kind == ValueEntry::VarValue && ctx->dirty[entry.constant] == 0 && ctx->numericTagged[entry.constant] != 1) {
        if (const Value *text = untouchedTextValue(ctx, entry.constant)) return *text;
    }
    return peekEntry(ctx, entry, position, scratch);
}

BytecodeJitLir::Word LIR_FUNC jitCompareValues(Context *ctx, const CompareDescriptor *compare) {
    if (double x, y; entryNumber(ctx, compare->a, compare->aPosition, x) && entryNumber(ctx, compare->b, compare->bPosition, y) && x == x && y == y) {
        switch (compare->kind) {
        case BytecodeCompareKind::Equals:
            return x == y;
        case BytecodeCompareKind::Less:
            return x < y;
        default:
            return x > y;
        }
    }
    Value scratchA, scratchB;
    const Value &a = compareOperand(ctx, compare->a, compare->aPosition, scratchA);
    const Value &b = compareOperand(ctx, compare->b, compare->bPosition, scratchB);
    const auto compute = [&] {
        switch (compare->kind) {
        case BytecodeCompareKind::Equals:
            return a == b;
        case BytecodeCompareKind::Less:
            return a < b;
        default:
            return a > b;
        }
    };
    if (a.isString() && b.isString()) {
        if (compare->memoValid && compare->memoA.stringId() == a.stringId() && compare->memoB.stringId() == b.stringId()) return compare->memoResult;
        const bool result = compute();
        compare->memoA = a;
        compare->memoB = b;
        compare->memoResult = result;
        compare->memoValid = true;
        return result;
    }
    return compute();
}

void LIR_FUNC jitListReplace(Context *ctx, const ListOpDescriptor *op) {
    List *list = listAt(*ctx->thread, op->list);
    if (list == nullptr || list->items.empty()) return;
    const size_t position = listPosition(ctx->slots[ctx->code->slotValues + op->indexPosition], list->items.size());
    if (position < list->items.size()) list->items[position] = materializeEntry(ctx, op->item, op->itemPosition);
}

void LIR_FUNC jitListAdd(Context *ctx, const ListOpDescriptor *op) {
    List *list = listAt(*ctx->thread, op->list);
    Value item = materializeEntry(ctx, op->item, op->itemPosition);
    if (list != nullptr && list->items.size() < BYTECODE_MAX_LIST_ITEMS) list->items.push_back(std::move(item));
}

void LIR_FUNC jitListInsert(Context *ctx, const ListOpDescriptor *op) {
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

void LIR_FUNC jitListDelete(Context *ctx, const ListOpDescriptor *op) {
    List *list = listAt(*ctx->thread, op->list);
    if (list == nullptr) return;
    double index = ctx->slots[ctx->code->slotValues + op->indexPosition];
    if (std::isnan(index)) index = 0.0;
    if (!std::isfinite(index)) return;
    const double position = std::floor(index) - 1;
    if (position >= 0 && position < static_cast<double>(list->items.size())) eraseValueAt(list->items, static_cast<size_t>(position));
}

void LIR_FUNC jitListClear(Context *ctx, const ListOpDescriptor *op) {
    if (List *list = listAt(*ctx->thread, op->list)) list->items.clear();
}

double LIR_FUNC jitListLength(Context *ctx, const ListOpDescriptor *op) {
    const List *list = listAt(*ctx->thread, op->list);
    return list != nullptr ? static_cast<double>(list->items.size()) : 0.0;
}

double LIR_FUNC jitListIndexOf(Context *ctx, const ListOpDescriptor *op) {
    const List *list = listAt(*ctx->thread, op->list);
    Value scratch;
    const Value &item = peekEntry(ctx, op->item, op->itemPosition, scratch);
    if (list == nullptr) return 0.0;
    const size_t found = bytecodeFindInList(list->items, item);
    return found < list->items.size() ? static_cast<double>(found + 1) : 0.0;
}

BytecodeJitLir::Word LIR_FUNC jitListContains(Context *ctx, const ListOpDescriptor *op) {
    const List *list = listAt(*ctx->thread, op->list);
    Value scratch;
    const Value &item = peekEntry(ctx, op->item, op->itemPosition, scratch);
    if (list == nullptr) return 0;
    return bytecodeFindInList(list->items, item) < list->items.size() ? 1 : 0;
}

} // namespace BytecodeJitInternal::Runtime

#endif
