#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "bytecode.hpp"
#include "bytecode_analysis.hpp"
#include "jit_emitter.hpp"
#include "jit_runtime.hpp"
#include "jit_types.hpp"
#include <cstdint>
#include <deque>
#include <functional>
#include <sljitLir.h>

namespace BytecodeJitInternal {

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
        else if (type == BytecodeValueType::Boolean) v.kind = ValueEntry::Boolean;
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
    sljit_emit_op2(c_, SLJIT_SUB32 | SLJIT_SET_Z, SLJIT_S2, 0, SLJIT_S2, 0, SLJIT_IMM, 1);
    pendJump(sljit_emit_jump(c_, SLJIT_NOT_ZERO), target);
    sljit_emit_op1(c_, SLJIT_MOV32, SLJIT_S2, 0, SLJIT_IMM, pollInterval());
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
    callHelper(reinterpret_cast<void *>(&Runtime::jitPreempt), SLJIT_ARGS1(W, P));
    sljit_jump *keepGoing = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
    emitExitAt(target, static_cast<uint32_t>(BytecodeJit::Result::Yield), stateAtTarget);
    sljit_set_label(keepGoing, sljit_emit_label(c_));
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

} // namespace BytecodeJitInternal

#endif
