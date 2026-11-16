#ifdef SE_JIT

#include "jit_emitter.hpp"
#include "jit_runtime.hpp"
#include "jit_types.hpp"
#include <cstdint>
#include <cstring>
#include <vector>

namespace BytecodeJitInternal {

bool Emitter::emitInlineAppend(StringOp &op) {
    const Runtime::StringLayout &layout = Runtime::stringLayout();
    if (!wide || !layout.ok || op.a.kind != StrEntry::Const || op.a.constant >= chunk_.stringConstants.size() || op.variable >= maxPromoted) return false;
    const std::string &literal = chunk_.stringConstants[op.a.constant];
    if (literal.empty() || literal.size() > 7) return false;

    const uint16_t endianProbe = 1;
    const bool little = *reinterpret_cast<const uint8_t *>(&endianProbe) == 1;
    uint64_t bits = 0;
    for (size_t i = 0; i < literal.size(); i++) {
        const uint64_t byte = static_cast<unsigned char>(literal[i]);
        bits |= little ? byte << (8 * i) : byte << (8 * (7 - i));
    }

    const uint32_t k = op.variable;
    std::vector<lir::Jump *> slow;
    slow.push_back(lir::emitCmp(c_, lir::NOT_EQUAL, lir::MEM1(lir::S0), dirtyOffset(k), lir::IMM, 4));
    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, reinterpret_cast<lir::Word>(&Runtime::stringVariables[k]));
    lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::MEM1(lir::R0), layout.size);
    slow.push_back(lir::emitCmp(c_, lir::EQUAL, lir::R1, 0, lir::IMM, 0));
    lir::emitOp1(c_, lir::MOV, lir::R2, 0, lir::MEM1(lir::R0), layout.pointer);
    lir::emitOp2(c_, lir::ADD, lir::R3, 0, lir::R0, 0, lir::IMM, layout.buffer);
    lir::emitOp1(c_, lir::MOV, lir::R4, 0, lir::IMM, layout.smallCapacity);
    lir::Jump *inBuffer = lir::emitCmp(c_, lir::EQUAL, lir::R2, 0, lir::R3, 0);
    lir::emitOp1(c_, lir::MOV, lir::R4, 0, lir::MEM1(lir::R0), layout.capacity);
    lir::setLabel(inBuffer, lir::emitLabel(c_));
    lir::emitOp2(c_, lir::ADD, lir::R3, 0, lir::R1, 0, lir::IMM, 7);
    slow.push_back(lir::emitCmp(c_, lir::GREATER, lir::R3, 0, lir::R4, 0));
    lir::emitOp2(c_, lir::ADD, lir::R2, 0, lir::R2, 0, lir::R1, 0);
    lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::R2), 0, lir::IMM, static_cast<lir::Word>(bits));
    lir::emitOp2(c_, lir::ADD, lir::R1, 0, lir::R1, 0, lir::IMM, static_cast<lir::Word>(literal.size()));
    lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::R0), layout.size, lir::R1, 0);
    lir::emitOp1(c_, lir::MOV, lir::R2, 0, lir::IMM, reinterpret_cast<lir::Word>(&Runtime::stringAscii[k]));
    lir::emitOp1(c_, lir::MOV_U8, lir::R3, 0, lir::MEM1(lir::R2), 0);
    lir::Jump *notAscii = lir::emitCmp(c_, lir::NOT_EQUAL, lir::R3, 0, lir::IMM, 1);
    lir::emitOp1(c_, lir::MOV_U8, lir::MEM1(lir::R2), 0, lir::IMM, 0xff);
    lir::setLabel(notAscii, lir::emitLabel(c_));
    lir::emitOp1(c_, lir::MOV, lir::R2, 0, lir::IMM, reinterpret_cast<lir::Word>(&Runtime::stringChar[k]));
    lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::R2), 0, lir::IMM, -1);
    lir::Label *done = lir::emitLabel(c_);

    addCold([this, slow, done, &op]() {
        lir::Label *here = lir::emitLabel(c_);
        for (lir::Jump *jump : slow)
            lir::setLabel(jump, here);
        callStringOp(op);
        lir::setLabel(lir::emitJump(c_, lir::JUMP), done);
    });
    return true;
}

bool Emitter::emitInlineLetterEquals(StringOp &letter, BytecodeIndex pc) {
    const Runtime::StringLayout &layout = Runtime::stringLayout();
    if (!wide || !layout.ok || pc + 2 >= chunk_.instructions.size() || letter.variable >= maxPromoted) return false;
    const BytecodeInstruction &push = chunk_.instructions[pc + 1];
    const BytecodeInstruction &compare = chunk_.instructions[pc + 2];
    if (push.op != BytecodeOp::PushConstString || compare.op != BytecodeOp::StrEquals || fused_[pc + 1] || fused_[pc + 2]) return false;
    if (!plan_.supported[pc + 1] || !plan_.supported[pc + 2] || analysis_.isBlockStart(pc + 1) || analysis_.isBlockStart(pc + 2) || plan_.entryOf[pc + 1] >= 0 || plan_.entryOf[pc + 2] >= 0) return false;
    if (push.a >= chunk_.stringConstants.size()) return false;
    const std::string &text = chunk_.stringConstants[push.a];
    if (text.size() != 1) return false;
    const unsigned char c = static_cast<unsigned char>(text[0]);
    if (c <= 0x20 || c >= 0x7F || (c >= '0' && c <= '9') || c == '.') return false;
    const bool isLetter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    const lir::Word literal = isLetter ? (c | 0x20) : c;

    const uint32_t k = letter.variable;
    const uint32_t out = booleanSlot(state_.booleans);
    StringOp &replay = newStringOp(StringOp::Compare);
    replay.compare = BytecodeCompareKind::Equals;
    replay.a.kind = StrEntry::Temp;
    replay.aPosition = letter.outPosition;
    replay.b.kind = StrEntry::Const;
    replay.b.constant = push.a;
    replay.bPosition = letter.outPosition + 1;
    replay.numberOut = out;

    std::vector<lir::Jump *> slow;
    slow.push_back(lir::emitCmp(c_, lir::NOT_EQUAL, lir::MEM1(lir::S0), dirtyOffset(k), lir::IMM, 4));
    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, reinterpret_cast<lir::Word>(&Runtime::stringAscii[k]));
    lir::emitOp1(c_, lir::MOV_U8, lir::R1, 0, lir::MEM1(lir::R0), 0);
    slow.push_back(lir::emitCmp(c_, lir::NOT_EQUAL, lir::R1, 0, lir::IMM, 1));
    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, reinterpret_cast<lir::Word>(&Runtime::stringVariables[k]));
    lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::MEM1(lir::R0), layout.size);
    lir::emitOp1(c_, lir::MOV, lir::R2, 0, lir::MEM1(lir::R0), layout.pointer);
    loadF(lir::FR0, letter.numberIn);
    lir::emitFOp1(c_, lir::CONV_SW_FROM_F64, lir::R3, 0, lir::FR0, 0);
    lir::emitOp2(c_, lir::SUB, lir::R3, 0, lir::R3, 0, lir::IMM, 1);
    slow.push_back(lir::emitCmp(c_, lir::GREATER_EQUAL, lir::R3, 0, lir::R1, 0));
    lir::emitOp1(c_, lir::MOV_U8, lir::R4, 0, lir::MEM2(lir::R2, lir::R3), 0);
    if (isLetter) lir::emitOp2(c_, lir::OR, lir::R4, 0, lir::R4, 0, lir::IMM, 0x20);
    lir::emitOp2U(c_, lir::SUB | lir::SET_Z, lir::R4, 0, lir::IMM, literal);
    storeFlag(out, lir::EQUAL);
    lir::Label *done = lir::emitLabel(c_);

    addCold([this, slow, done, &letter, &replay]() {
        lir::Label *here = lir::emitLabel(c_);
        for (lir::Jump *jump : slow)
            lir::setLabel(jump, here);
        callStringOp(letter);
        callStringOp(replay);
        lir::setLabel(lir::emitJump(c_, lir::JUMP), done);
    });

    fused_[pc + 1] = true;
    fused_[pc + 2] = true;
    state_.booleans++;
    maxBooleans_ = std::max(maxBooleans_, state_.booleans);
    return true;
}

} // namespace BytecodeJitInternal

#endif
