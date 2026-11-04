#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "bytecode.hpp"
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
#include <sljitLir.h>
#include <utility>

namespace BytecodeJitInternal::Runtime {

void SLJIT_FUNC jitListItem(Context *ctx, const ListOpDescriptor *op) {
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
    temps[op->indexPosition] = std::move(item);
    tempLazy[op->indexPosition] = 0;
    ctx->tempHigh = std::max(ctx->tempHigh, op->indexPosition + 1);
}

void SLJIT_FUNC jitStoreShadow(Context *ctx, const ShadowDescriptor *shadow) {
    shadowValues[shadow->variable] = materializeEntry(ctx, shadow->entry, shadow->position);
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

} // namespace BytecodeJitInternal::Runtime

#endif
