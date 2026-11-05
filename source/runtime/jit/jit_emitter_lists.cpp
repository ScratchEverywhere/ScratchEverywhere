#ifdef SE_JIT

#include "jit_emitter.hpp"
#include "jit_runtime.hpp"
#include "jit_types.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <sljitLir.h>
#include <vector>

namespace BytecodeJitInternal {

void Emitter::emitListItemAddress(uint32_t slot, const Operand &index, std::vector<sljit_jump *> &slow) {
    const Runtime::ListLayout &layout = Runtime::listLayout();
    loadOperand(SLJIT_FR0, index);
    sljit_emit_fop1(c_, SLJIT_CONV_SW_FROM_F64, SLJIT_R2, 0, SLJIT_FR0, 0);
    sljit_emit_op2(c_, SLJIT_SUB, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, 1);
    slow.push_back(sljit_emit_cmp(c_, SLJIT_GREATER_EQUAL, SLJIT_R2, 0, SLJIT_IMM, 0x7fffffff));
    sljit_emit_op2(c_, SLJIT_MUL, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, layout.valueSize);
    if (active_ != nullptr && active_->hoistList == static_cast<int64_t>(slot)) {
        sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R2, 0, SLJIT_S4, 0);
        slow.push_back(sljit_emit_cmp(c_, SLJIT_GREATER_EQUAL, SLJIT_R1, 0, SLJIT_S5, 0));
        return;
    }
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, lists));
    slow.push_back(sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0));
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R0), static_cast<sljit_sw>(slot) * static_cast<sljit_sw>(sizeof(void *)));
    slow.push_back(sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0));
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R0), layout.items);
    sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R2, 0);
    slow.push_back(sljit_emit_cmp(c_, SLJIT_GREATER_EQUAL, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R0), layout.items + static_cast<sljit_sw>(sizeof(void *))));
}

bool Emitter::emitInlineListItem(const ListOpDescriptor &op, const std::vector<sljit_jump *> &notPlain, void *helper, const Operand *index, bool carry) {
    const Runtime::ListLayout &layout = Runtime::listLayout();
    if (!layout.ok) return false;
    if (helper == nullptr) helper = reinterpret_cast<void *>(&Runtime::jitListItem);
    const uint32_t out = valueSlot(op.indexPosition);
    std::vector<sljit_jump *> slow = notPlain;
    const Operand indexSource = index != nullptr ? *index : Operand{false, out, 0};
    emitListItemAddress(op.list, indexSource, slow);

    const uint16_t endianProbe = 1;
    const bool little = *reinterpret_cast<const uint8_t *>(&endianProbe) == 1;
    const bool combined = layout.flags == layout.tag + 1;
    sljit_jump *flagged;
    if (combined) {
        sljit_emit_op1(c_, SLJIT_MOV_U16, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R1), layout.tag);
        flagged = sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, little ? layout.numberTag : layout.numberTag << 8);
    } else {
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R1), layout.tag);
        slow.push_back(sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, layout.numberTag));
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R1), layout.flags);
        flagged = sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, 0);
    }
    sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_R1), layout.number);
    if (!carry) {
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_MEM0(), reinterpret_cast<sljit_sw>(&Runtime::tempLazy[op.indexPosition]), SLJIT_IMM, 1);
        storeF(out, SLJIT_FR0);
    }
    sljit_label *done = sljit_emit_label(c_);

    const bool indexNotStored = index != nullptr;
    addCold([this, slow, flagged, done, &op, out, layout, helper, indexSource, indexNotStored, combined]() {
        const auto storeIndex = [&]() {
            if (!indexNotStored) return;
            loadOperand(SLJIT_FR0, indexSource);
            storeF(out, SLJIT_FR0);
        };
        sljit_label *here = sljit_emit_label(c_);
        for (sljit_jump *jump : slow)
            sljit_set_label(jump, here);
        storeIndex();
        callListHelper(helper, const_cast<ListOpDescriptor &>(op), SLJIT_ARGS2V(P, P));
        loadF(SLJIT_FR0, out);
        sljit_jump *back = sljit_emit_jump(c_, SLJIT_JUMP);
        sljit_set_label(back, done);

        sljit_set_label(flagged, sljit_emit_label(c_));
        if (combined) {
            sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R1), layout.tag);
            sljit_set_label(sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, layout.numberTag), here);
        }
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&Runtime::temps[op.indexPosition]));
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R0), layout.tag);
        sljit_jump *notString = sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, layout.stringTag);
        sljit_emit_op1(c_, SLJIT_MOV_U32, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R0), layout.number);
        sljit_jump *pooled = sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, 0);
        sljit_set_label(notString, sljit_emit_label(c_));
        sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_R1), layout.number);
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R1), layout.flags);
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_R0), layout.flags, SLJIT_R2, 0);
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_R0), layout.tag, SLJIT_IMM, layout.numberTag);
        sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), layout.number, SLJIT_FR0, 0);
        sljit_jump *otherFlags = sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, layout.typedFlags);
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_MEM0(), reinterpret_cast<sljit_sw>(&Runtime::tempLazy[op.indexPosition]), SLJIT_IMM, 2);
        sljit_jump *marked = sljit_emit_jump(c_, SLJIT_JUMP);
        sljit_set_label(otherFlags, sljit_emit_label(c_));
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_MEM0(), reinterpret_cast<sljit_sw>(&Runtime::tempLazy[op.indexPosition]), SLJIT_IMM, 0);
        sljit_set_label(marked, sljit_emit_label(c_));
        storeF(out, SLJIT_FR0);
        sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), done);
        sljit_set_label(pooled, sljit_emit_label(c_));
        storeIndex();
        callListHelper(helper, const_cast<ListOpDescriptor &>(op), SLJIT_ARGS2V(P, P));
        loadF(SLJIT_FR0, out);
        sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), done);
    });
    return true;
}

bool Emitter::emitInlineListReplace(const ListOpDescriptor &op, const AbsValue &item) {
    const Runtime::ListLayout &layout = Runtime::listLayout();
    const bool variable = item.kind == ValueEntry::VarValue && item.constant < code_.promoted.size();
    const bool temporary = item.kind == ValueEntry::Temp;
    const bool typedConstant = item.kind == ValueEntry::Constant && item.constant < chunk_.constants.size() && chunk_.constants[item.constant].isStringTyped() && !chunk_.constants[item.constant].isString();
    if (!layout.ok || (item.kind != ValueEntry::Number && !typedConstant && !variable && !temporary)) return false;
    std::vector<sljit_jump *> slow;
    emitListItemAddress(op.list, Operand{false, valueSlot(op.indexPosition), 0}, slow);
    if (variable) {
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_S0), dirtyOffset(item.constant));
        sljit_emit_op2(c_, SLJIT_SUB, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, 1);
        slow.push_back(sljit_emit_cmp(c_, SLJIT_GREATER, SLJIT_R2, 0, SLJIT_IMM, 1));
    }
    if (temporary) {
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R2, 0, SLJIT_MEM0(), reinterpret_cast<sljit_sw>(&Runtime::tempLazy[op.itemPosition]));
        sljit_jump *lazy = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, 1);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&Runtime::temps[op.itemPosition]));
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R0), layout.tag);
        slow.push_back(sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, layout.numberTag));
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R0), layout.flags);
        slow.push_back(sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, 0));
        sljit_set_label(lazy, sljit_emit_label(c_));
    }

    sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R1), layout.tag);
    slow.push_back(sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, layout.stringTag));
    loadF(SLJIT_FR2, valueSlot(op.itemPosition));
    const uint16_t endianProbe = 1;
    const bool little = *reinterpret_cast<const uint8_t *>(&endianProbe) == 1;
    if (layout.flags == layout.tag + 1) {
        const sljit_sw flags = typedConstant ? layout.typedFlags : 0;
        sljit_emit_op1(c_, SLJIT_MOV_U16, SLJIT_MEM1(SLJIT_R1), layout.tag, SLJIT_IMM, little ? (layout.numberTag | (flags << 8)) : ((layout.numberTag << 8) | flags));
    } else {
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_R1), layout.tag, SLJIT_IMM, layout.numberTag);
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_R1), layout.flags, SLJIT_IMM, typedConstant ? layout.typedFlags : 0);
    }
    sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R1), layout.number, SLJIT_FR2, 0);
    sljit_label *done = sljit_emit_label(c_);

    addCold([this, slow, done, &op]() {
        sljit_label *here = sljit_emit_label(c_);
        for (sljit_jump *jump : slow)
            sljit_set_label(jump, here);
        callListHelper(reinterpret_cast<void *>(&Runtime::jitListReplace), const_cast<ListOpDescriptor &>(op), SLJIT_ARGS2V(P, P));
        sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), done);
    });
    return true;
}

bool Emitter::emitInlineListAdd(const ListOpDescriptor &op, const AbsValue &item) {
    const Runtime::ListLayout &layout = Runtime::listLayout();
    const bool isBoolean = item.kind == ValueEntry::Boolean;
    const bool variable = item.kind == ValueEntry::VarValue && item.constant < code_.promoted.size();
    const bool temporary = item.kind == ValueEntry::Temp;
    const bool typedConstant = item.kind == ValueEntry::Constant && item.constant < chunk_.constants.size() && chunk_.constants[item.constant].isStringTyped() && !chunk_.constants[item.constant].isString();
    if (!layout.ok || (item.kind != ValueEntry::Number && !isBoolean && !typedConstant && !variable && !temporary)) return false;
    std::vector<sljit_jump *> slow;
    if (variable) {
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_S0), dirtyOffset(item.constant));
        sljit_emit_op2(c_, SLJIT_SUB, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, 1);
        slow.push_back(sljit_emit_cmp(c_, SLJIT_GREATER, SLJIT_R2, 0, SLJIT_IMM, 1));
    }
    if (temporary) {
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R2, 0, SLJIT_MEM0(), reinterpret_cast<sljit_sw>(&Runtime::tempLazy[op.itemPosition]));
        sljit_jump *lazy = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, 1);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&Runtime::temps[op.itemPosition]));
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R0), layout.tag);
        slow.push_back(sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, layout.numberTag));
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R0), layout.flags);
        slow.push_back(sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, 0));
        sljit_set_label(lazy, sljit_emit_label(c_));
    }

    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, lists));
    slow.push_back(sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0));
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R0), static_cast<sljit_sw>(op.list) * static_cast<sljit_sw>(sizeof(void *)));
    slow.push_back(sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0));
    const sljit_sw pointer = static_cast<sljit_sw>(sizeof(void *));
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R0), layout.items + pointer);
    slow.push_back(sljit_emit_cmp(c_, SLJIT_GREATER_EQUAL, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R0), layout.items + 2 * pointer));
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R0), layout.items);
    sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, static_cast<sljit_sw>(BYTECODE_MAX_LIST_ITEMS) * layout.valueSize);
    slow.push_back(sljit_emit_cmp(c_, SLJIT_GREATER_EQUAL, SLJIT_R1, 0, SLJIT_R2, 0));

    const uint16_t endianProbe = 1;
    const bool little = *reinterpret_cast<const uint8_t *>(&endianProbe) == 1;
    if (isBoolean) {
        if (wide) {
            loadW(SLJIT_R2, valueSlot(op.itemPosition));
        } else {
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_S0), highWord(slotOffset(valueSlot(op.itemPosition))));
            sljit_emit_op2(c_, SLJIT_OR, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_S0), lowWord(slotOffset(valueSlot(op.itemPosition))));
        }
        sljit_emit_op2u(c_, SLJIT_SUB | SLJIT_SET_Z, SLJIT_R2, 0, SLJIT_IMM, 0);
        sljit_emit_op_flags(c_, SLJIT_MOV, SLJIT_R2, 0, SLJIT_NOT_EQUAL);
        if (layout.flags == layout.tag + 1) {
            sljit_emit_op1(c_, SLJIT_MOV_U16, SLJIT_MEM1(SLJIT_R1), layout.tag, SLJIT_IMM, little ? layout.booleanTag : (layout.booleanTag << 8));
        } else {
            sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_R1), layout.tag, SLJIT_IMM, layout.booleanTag);
            sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_R1), layout.flags, SLJIT_IMM, 0);
        }
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), layout.number, SLJIT_IMM, 0);
        if (!wide) {
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_R1), layout.number + static_cast<sljit_sw>(sizeof(sljit_sw)), SLJIT_IMM, 0);
        }
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_R1), layout.number, SLJIT_R2, 0);
    } else {
        loadF(SLJIT_FR2, valueSlot(op.itemPosition));
        if (layout.flags == layout.tag + 1) {
            const sljit_sw flags = typedConstant ? layout.typedFlags : 0;
            sljit_emit_op1(c_, SLJIT_MOV_U16, SLJIT_MEM1(SLJIT_R1), layout.tag, SLJIT_IMM, little ? (layout.numberTag | (flags << 8)) : ((layout.numberTag << 8) | flags));
        } else {
            sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_R1), layout.tag, SLJIT_IMM, layout.numberTag);
            sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_R1), layout.flags, SLJIT_IMM, typedConstant ? layout.typedFlags : 0);
        }
        sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R1), layout.number, SLJIT_FR2, 0);
    }
    sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, layout.valueSize);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_R0), layout.items + pointer, SLJIT_R1, 0);
    sljit_label *done = sljit_emit_label(c_);

    addCold([this, slow, done, &op]() {
        sljit_label *here = sljit_emit_label(c_);
        for (sljit_jump *jump : slow)
            sljit_set_label(jump, here);
        callListHelper(reinterpret_cast<void *>(&Runtime::jitListAdd), const_cast<ListOpDescriptor &>(op), SLJIT_ARGS2V(P, P));
        sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), done);
    });
    return true;
}

} // namespace BytecodeJitInternal

#endif
