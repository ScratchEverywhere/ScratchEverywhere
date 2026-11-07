#pragma once
#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "lir_caps.hpp"
#include "lir_isa.hpp"
#include <deque>
#include <vector>

namespace BytecodeJitLir {

class Builder;

struct Jump {
    void *real = nullptr;
    Builder *owner = nullptr;
    int id = 0;
};

struct Label {
    void *real = nullptr;
    int id = 0;
};

enum class Kind : uint8_t { Enter,
                            Return,
                            Op0,
                            Op1,
                            Op2,
                            Op2U,
                            OpFlags,
                            FOp1,
                            FOp2,
                            FSet64,
                            IJump,
                            ICall,
                            JumpOp,
                            Cmp,
                            FCmp,
                            LabelOp,
                            SetLabel };

struct Instr {
    Kind kind;
    Op op = 0;
    Sig sig = 0;
    Opnd dst = NONE, src1 = NONE, src2 = NONE;
    Word dstw = 0, src1w = 0, src2w = 0;
    Int counts[4] = {0, 0, 0, 0};
    double value = 0;
    Jump *jump = nullptr;
    Label *label = nullptr;
};

class Builder {
  public:
    std::vector<Instr> code;
    std::deque<Jump> jumps;
    std::deque<Label> labels;
    void *machine = nullptr;
    size_t machineSize = 0;
    void *backend = nullptr;
};

Builder *createCompiler();
void freeCompiler(Builder *b);
void *generateCode(Builder *b);
UWord labelAddress(Label *label);

Int emitEnter(Builder *b, Sig sig, Int scratchRegisters, Int savedRegisters, Int floatRegisters, Int localSize);
Int emitReturn(Builder *b, Op op, Opnd src, Word srcw);
Int emitOp0(Builder *b, Op op);
Int emitOp1(Builder *b, Op op, Opnd dst, Word dstw, Opnd src, Word srcw);
Int emitOp2(Builder *b, Op op, Opnd dst, Word dstw, Opnd src1, Word src1w, Opnd src2, Word src2w);
Int emitOp2U(Builder *b, Op op, Opnd src1, Word src1w, Opnd src2, Word src2w);
Int emitOpFlags(Builder *b, Op op, Opnd dst, Word dstw, Cond type);
Int emitFOp1(Builder *b, Op op, Opnd dst, Word dstw, Opnd src, Word srcw);
Int emitFOp2(Builder *b, Op op, Opnd dst, Word dstw, Opnd src1, Word src1w, Opnd src2, Word src2w);
Int emitFSet64(Builder *b, Opnd freg, double value);
Int emitIJump(Builder *b, Cond type, Opnd src, Word srcw);
Int emitICall(Builder *b, Cond type, Sig sig, Opnd src, Word srcw);
Jump *emitJump(Builder *b, Cond type);
Jump *emitCmp(Builder *b, Cond type, Opnd src1, Word src1w, Opnd src2, Word src2w);
Jump *emitFCmp(Builder *b, Cond type, Opnd src1, Word src1w, Opnd src2, Word src2w);
Label *emitLabel(Builder *b);
void setLabel(Jump *jump, Label *label);

void *generateWithSljit(Builder &b);
void releaseSljit(Builder &b);

} // namespace BytecodeJitLir

#endif
