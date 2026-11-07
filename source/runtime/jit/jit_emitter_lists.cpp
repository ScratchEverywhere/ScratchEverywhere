#ifdef SE_JIT

#include "jit_emitter.hpp"
#include "jit_runtime.hpp"
#include "jit_types.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace BytecodeJitInternal {

void Emitter::emitListItemAddress(uint32_t slot, const Operand &index, std::vector<lir::Jump *> &slow) {
    const Runtime::ListLayout &layout = Runtime::listLayout();
    loadOperand(lir::FR0, index);
    lir::emitFOp1(c_, lir::CONV_SW_FROM_F64, lir::R2, 0, lir::FR0, 0);
    lir::emitOp2(c_, lir::SUB, lir::R2, 0, lir::R2, 0, lir::IMM, 1);
    slow.push_back(lir::emitCmp(c_, lir::GREATER_EQUAL, lir::R2, 0, lir::IMM, 0x7fffffff));
    lir::emitOp2(c_, lir::MUL, lir::R2, 0, lir::R2, 0, lir::IMM, layout.valueSize);
    if (active_ != nullptr && active_->hoistList == static_cast<int64_t>(slot)) {
        lir::emitOp2(c_, lir::ADD, lir::R1, 0, lir::R2, 0, lir::S4, 0);
        slow.push_back(lir::emitCmp(c_, lir::GREATER_EQUAL, lir::R1, 0, lir::S5, 0));
        return;
    }
    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::MEM1(lir::S0), offsetof(Context, lists));
    slow.push_back(lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 0));
    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::MEM1(lir::R0), static_cast<lir::Word>(slot) * static_cast<lir::Word>(sizeof(void *)));
    slow.push_back(lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 0));
    lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::MEM1(lir::R0), layout.items);
    lir::emitOp2(c_, lir::ADD, lir::R1, 0, lir::R1, 0, lir::R2, 0);
    slow.push_back(lir::emitCmp(c_, lir::GREATER_EQUAL, lir::R1, 0, lir::MEM1(lir::R0), layout.items + static_cast<lir::Word>(sizeof(void *))));
}

bool Emitter::emitInlineListItem(const ListOpDescriptor &op, const std::vector<lir::Jump *> &notPlain, void *helper, const Operand *index, bool carry) {
    const Runtime::ListLayout &layout = Runtime::listLayout();
    if (!layout.ok) return false;
    if (helper == nullptr) helper = reinterpret_cast<void *>(&Runtime::jitListItem);
    const uint32_t out = valueSlot(op.indexPosition);
    std::vector<lir::Jump *> slow = notPlain;
    const Operand indexSource = index != nullptr ? *index : Operand{false, out, 0};
    emitListItemAddress(op.list, indexSource, slow);

    const uint16_t endianProbe = 1;
    const bool little = *reinterpret_cast<const uint8_t *>(&endianProbe) == 1;
    const bool combined = layout.flags == layout.tag + 1;
    lir::Jump *flagged;
    if (combined) {
        lir::emitOp1(c_, lir::MOV_U16, lir::R2, 0, lir::MEM1(lir::R1), layout.tag);
        flagged = lir::emitCmp(c_, lir::NOT_EQUAL, lir::R2, 0, lir::IMM, little ? layout.numberTag : layout.numberTag << 8);
    } else {
        lir::emitOp1(c_, lir::MOV_U8, lir::R2, 0, lir::MEM1(lir::R1), layout.tag);
        slow.push_back(lir::emitCmp(c_, lir::NOT_EQUAL, lir::R2, 0, lir::IMM, layout.numberTag));
        lir::emitOp1(c_, lir::MOV_U8, lir::R2, 0, lir::MEM1(lir::R1), layout.flags);
        flagged = lir::emitCmp(c_, lir::NOT_EQUAL, lir::R2, 0, lir::IMM, 0);
    }
    lir::emitFOp1(c_, lir::MOV_F64, lir::FR0, 0, lir::MEM1(lir::R1), layout.number);
    if (!carry) {
        lir::emitOp1(c_, lir::MOV_U8, lir::MEM0(), reinterpret_cast<lir::Word>(&Runtime::tempLazy[op.indexPosition]), lir::IMM, 1);
        storeF(out, lir::FR0);
    }
    lir::Label *done = lir::emitLabel(c_);

    const bool indexNotStored = index != nullptr;
    addCold([this, slow, flagged, done, &op, out, layout, helper, indexSource, indexNotStored, combined]() {
        const auto storeIndex = [&]() {
            if (!indexNotStored) return;
            loadOperand(lir::FR0, indexSource);
            storeF(out, lir::FR0);
        };
        lir::Label *here = lir::emitLabel(c_);
        for (lir::Jump *jump : slow)
            lir::setLabel(jump, here);
        storeIndex();
        callListHelper(helper, const_cast<ListOpDescriptor &>(op), LIR_ARGS2V(P, P));
        loadF(lir::FR0, out);
        lir::Jump *back = lir::emitJump(c_, lir::JUMP);
        lir::setLabel(back, done);

        lir::setLabel(flagged, lir::emitLabel(c_));
        if (combined) {
            lir::emitOp1(c_, lir::MOV_U8, lir::R2, 0, lir::MEM1(lir::R1), layout.tag);
            lir::setLabel(lir::emitCmp(c_, lir::NOT_EQUAL, lir::R2, 0, lir::IMM, layout.numberTag), here);
        }
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, reinterpret_cast<lir::Word>(&Runtime::temps[op.indexPosition]));
        lir::emitOp1(c_, lir::MOV_U8, lir::R2, 0, lir::MEM1(lir::R0), layout.tag);
        lir::Jump *notString = lir::emitCmp(c_, lir::NOT_EQUAL, lir::R2, 0, lir::IMM, layout.stringTag);
        lir::emitOp1(c_, lir::MOV_U32, lir::R2, 0, lir::MEM1(lir::R0), layout.number);
        lir::Jump *pooled = lir::emitCmp(c_, lir::NOT_EQUAL, lir::R2, 0, lir::IMM, 0);
        lir::setLabel(notString, lir::emitLabel(c_));
        lir::emitFOp1(c_, lir::MOV_F64, lir::FR0, 0, lir::MEM1(lir::R1), layout.number);
        lir::emitOp1(c_, lir::MOV_U8, lir::R2, 0, lir::MEM1(lir::R1), layout.flags);
        lir::emitOp1(c_, lir::MOV_U8, lir::MEM1(lir::R0), layout.flags, lir::R2, 0);
        lir::emitOp1(c_, lir::MOV_U8, lir::MEM1(lir::R0), layout.tag, lir::IMM, layout.numberTag);
        lir::emitFOp1(c_, lir::MOV_F64, lir::MEM1(lir::R0), layout.number, lir::FR0, 0);
        lir::Jump *otherFlags = lir::emitCmp(c_, lir::NOT_EQUAL, lir::R2, 0, lir::IMM, layout.typedFlags);
        lir::emitOp1(c_, lir::MOV_U8, lir::MEM0(), reinterpret_cast<lir::Word>(&Runtime::tempLazy[op.indexPosition]), lir::IMM, 2);
        lir::Jump *marked = lir::emitJump(c_, lir::JUMP);
        lir::setLabel(otherFlags, lir::emitLabel(c_));
        lir::emitOp1(c_, lir::MOV_U8, lir::MEM0(), reinterpret_cast<lir::Word>(&Runtime::tempLazy[op.indexPosition]), lir::IMM, 0);
        lir::setLabel(marked, lir::emitLabel(c_));
        storeF(out, lir::FR0);
        lir::setLabel(lir::emitJump(c_, lir::JUMP), done);
        lir::setLabel(pooled, lir::emitLabel(c_));
        storeIndex();
        callListHelper(helper, const_cast<ListOpDescriptor &>(op), LIR_ARGS2V(P, P));
        loadF(lir::FR0, out);
        lir::setLabel(lir::emitJump(c_, lir::JUMP), done);
    });
    return true;
}

bool Emitter::emitInlineListReplace(const ListOpDescriptor &op, const AbsValue &item) {
    const Runtime::ListLayout &layout = Runtime::listLayout();
    const bool variable = item.kind == ValueEntry::VarValue && item.constant < code_.promoted.size();
    const bool temporary = item.kind == ValueEntry::Temp;
    const bool typedConstant = item.kind == ValueEntry::Constant && item.constant < chunk_.constants.size() && chunk_.constants[item.constant].isStringTyped() && !chunk_.constants[item.constant].isString();
    if (!layout.ok || (item.kind != ValueEntry::Number && !typedConstant && !variable && !temporary)) return false;
    std::vector<lir::Jump *> slow;
    emitListItemAddress(op.list, Operand{false, valueSlot(op.indexPosition), 0}, slow);
    if (variable) {
        lir::emitOp1(c_, lir::MOV, lir::R2, 0, lir::MEM1(lir::S0), dirtyOffset(item.constant));
        lir::emitOp2(c_, lir::SUB, lir::R2, 0, lir::R2, 0, lir::IMM, 1);
        slow.push_back(lir::emitCmp(c_, lir::GREATER, lir::R2, 0, lir::IMM, 1));
    }
    if (temporary) {
        lir::emitOp1(c_, lir::MOV_U8, lir::R2, 0, lir::MEM0(), reinterpret_cast<lir::Word>(&Runtime::tempLazy[op.itemPosition]));
        lir::Jump *lazy = lir::emitCmp(c_, lir::EQUAL, lir::R2, 0, lir::IMM, 1);
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, reinterpret_cast<lir::Word>(&Runtime::temps[op.itemPosition]));
        lir::emitOp1(c_, lir::MOV_U8, lir::R2, 0, lir::MEM1(lir::R0), layout.tag);
        slow.push_back(lir::emitCmp(c_, lir::NOT_EQUAL, lir::R2, 0, lir::IMM, layout.numberTag));
        lir::emitOp1(c_, lir::MOV_U8, lir::R2, 0, lir::MEM1(lir::R0), layout.flags);
        slow.push_back(lir::emitCmp(c_, lir::NOT_EQUAL, lir::R2, 0, lir::IMM, 0));
        lir::setLabel(lazy, lir::emitLabel(c_));
    }

    lir::emitOp1(c_, lir::MOV_U8, lir::R2, 0, lir::MEM1(lir::R1), layout.tag);
    slow.push_back(lir::emitCmp(c_, lir::EQUAL, lir::R2, 0, lir::IMM, layout.stringTag));
    loadF(lir::FR2, valueSlot(op.itemPosition));
    const uint16_t endianProbe = 1;
    const bool little = *reinterpret_cast<const uint8_t *>(&endianProbe) == 1;
    if (layout.flags == layout.tag + 1) {
        const lir::Word flags = typedConstant ? layout.typedFlags : 0;
        lir::emitOp1(c_, lir::MOV_U16, lir::MEM1(lir::R1), layout.tag, lir::IMM, little ? (layout.numberTag | (flags << 8)) : ((layout.numberTag << 8) | flags));
    } else {
        lir::emitOp1(c_, lir::MOV_U8, lir::MEM1(lir::R1), layout.tag, lir::IMM, layout.numberTag);
        lir::emitOp1(c_, lir::MOV_U8, lir::MEM1(lir::R1), layout.flags, lir::IMM, typedConstant ? layout.typedFlags : 0);
    }
    lir::emitFOp1(c_, lir::MOV_F64, lir::MEM1(lir::R1), layout.number, lir::FR2, 0);
    lir::Label *done = lir::emitLabel(c_);

    addCold([this, slow, done, &op]() {
        lir::Label *here = lir::emitLabel(c_);
        for (lir::Jump *jump : slow)
            lir::setLabel(jump, here);
        callListHelper(reinterpret_cast<void *>(&Runtime::jitListReplace), const_cast<ListOpDescriptor &>(op), LIR_ARGS2V(P, P));
        lir::setLabel(lir::emitJump(c_, lir::JUMP), done);
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
    std::vector<lir::Jump *> slow;
    if (variable) {
        lir::emitOp1(c_, lir::MOV, lir::R2, 0, lir::MEM1(lir::S0), dirtyOffset(item.constant));
        lir::emitOp2(c_, lir::SUB, lir::R2, 0, lir::R2, 0, lir::IMM, 1);
        slow.push_back(lir::emitCmp(c_, lir::GREATER, lir::R2, 0, lir::IMM, 1));
    }
    if (temporary) {
        lir::emitOp1(c_, lir::MOV_U8, lir::R2, 0, lir::MEM0(), reinterpret_cast<lir::Word>(&Runtime::tempLazy[op.itemPosition]));
        lir::Jump *lazy = lir::emitCmp(c_, lir::EQUAL, lir::R2, 0, lir::IMM, 1);
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, reinterpret_cast<lir::Word>(&Runtime::temps[op.itemPosition]));
        lir::emitOp1(c_, lir::MOV_U8, lir::R2, 0, lir::MEM1(lir::R0), layout.tag);
        slow.push_back(lir::emitCmp(c_, lir::NOT_EQUAL, lir::R2, 0, lir::IMM, layout.numberTag));
        lir::emitOp1(c_, lir::MOV_U8, lir::R2, 0, lir::MEM1(lir::R0), layout.flags);
        slow.push_back(lir::emitCmp(c_, lir::NOT_EQUAL, lir::R2, 0, lir::IMM, 0));
        lir::setLabel(lazy, lir::emitLabel(c_));
    }

    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::MEM1(lir::S0), offsetof(Context, lists));
    slow.push_back(lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 0));
    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::MEM1(lir::R0), static_cast<lir::Word>(op.list) * static_cast<lir::Word>(sizeof(void *)));
    slow.push_back(lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 0));
    const lir::Word pointer = static_cast<lir::Word>(sizeof(void *));
    lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::MEM1(lir::R0), layout.items + pointer);
    slow.push_back(lir::emitCmp(c_, lir::GREATER_EQUAL, lir::R1, 0, lir::MEM1(lir::R0), layout.items + 2 * pointer));
    lir::emitOp1(c_, lir::MOV, lir::R2, 0, lir::MEM1(lir::R0), layout.items);
    lir::emitOp2(c_, lir::ADD, lir::R2, 0, lir::R2, 0, lir::IMM, static_cast<lir::Word>(BYTECODE_MAX_LIST_ITEMS) * layout.valueSize);
    slow.push_back(lir::emitCmp(c_, lir::GREATER_EQUAL, lir::R1, 0, lir::R2, 0));

    const uint16_t endianProbe = 1;
    const bool little = *reinterpret_cast<const uint8_t *>(&endianProbe) == 1;
    if (isBoolean) {
        if (wide) {
            loadW(lir::R2, valueSlot(op.itemPosition));
        } else {
            lir::emitOp1(c_, lir::MOV, lir::R2, 0, lir::MEM1(lir::S0), highWord(slotOffset(valueSlot(op.itemPosition))));
            lir::emitOp2(c_, lir::OR, lir::R2, 0, lir::R2, 0, lir::MEM1(lir::S0), lowWord(slotOffset(valueSlot(op.itemPosition))));
        }
        lir::emitOp2U(c_, lir::SUB | lir::SET_Z, lir::R2, 0, lir::IMM, 0);
        lir::emitOpFlags(c_, lir::MOV, lir::R2, 0, lir::NOT_EQUAL);
        if (layout.flags == layout.tag + 1) {
            lir::emitOp1(c_, lir::MOV_U16, lir::MEM1(lir::R1), layout.tag, lir::IMM, little ? layout.booleanTag : (layout.booleanTag << 8));
        } else {
            lir::emitOp1(c_, lir::MOV_U8, lir::MEM1(lir::R1), layout.tag, lir::IMM, layout.booleanTag);
            lir::emitOp1(c_, lir::MOV_U8, lir::MEM1(lir::R1), layout.flags, lir::IMM, 0);
        }
        lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::R1), layout.number, lir::IMM, 0);
        if (!wide) {
            lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::R1), layout.number + static_cast<lir::Word>(sizeof(lir::Word)), lir::IMM, 0);
        }
        lir::emitOp1(c_, lir::MOV_U8, lir::MEM1(lir::R1), layout.number, lir::R2, 0);
    } else {
        loadF(lir::FR2, valueSlot(op.itemPosition));
        if (layout.flags == layout.tag + 1) {
            const lir::Word flags = typedConstant ? layout.typedFlags : 0;
            lir::emitOp1(c_, lir::MOV_U16, lir::MEM1(lir::R1), layout.tag, lir::IMM, little ? (layout.numberTag | (flags << 8)) : ((layout.numberTag << 8) | flags));
        } else {
            lir::emitOp1(c_, lir::MOV_U8, lir::MEM1(lir::R1), layout.tag, lir::IMM, layout.numberTag);
            lir::emitOp1(c_, lir::MOV_U8, lir::MEM1(lir::R1), layout.flags, lir::IMM, typedConstant ? layout.typedFlags : 0);
        }
        lir::emitFOp1(c_, lir::MOV_F64, lir::MEM1(lir::R1), layout.number, lir::FR2, 0);
    }
    lir::emitOp2(c_, lir::ADD, lir::R1, 0, lir::R1, 0, lir::IMM, layout.valueSize);
    lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::R0), layout.items + pointer, lir::R1, 0);
    lir::Label *done = lir::emitLabel(c_);

    addCold([this, slow, done, &op]() {
        lir::Label *here = lir::emitLabel(c_);
        for (lir::Jump *jump : slow)
            lir::setLabel(jump, here);
        callListHelper(reinterpret_cast<void *>(&Runtime::jitListAdd), const_cast<ListOpDescriptor &>(op), LIR_ARGS2V(P, P));
        lir::setLabel(lir::emitJump(c_, lir::JUMP), done);
    });
    return true;
}

} // namespace BytecodeJitInternal

#endif
