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

} // namespace BytecodeJitInternal

#endif
