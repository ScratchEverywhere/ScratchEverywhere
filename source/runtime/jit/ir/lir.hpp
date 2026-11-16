#pragma once
#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "lir_caps.hpp"
#include "lir_isa.hpp"
#include <deque>
#include <vector>

namespace BytecodeJitLir {

class Builder;
struct BackendOps;

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
    std::vector<Label *> entryLabels;
    Label *coldLabel = nullptr;
    const BackendOps *ops = nullptr;
};

struct Mark {
    size_t code = 0, jumps = 0, labels = 0;
};
Mark mark(const Builder *b);
void rewind(Builder *b, const Mark &to);

struct BackendOps {
    const char *name;
    bool (*usable)();
    void *(*generate)(Builder &b);
    void (*release)(Builder &b);
    void (*freeMachine)(void *machine);
    UWord (*labelAddress)(Label *label);
    bool needsLock;
};
extern const BackendOps sljitOps, asmjitOps, llvmOps;

const BackendOps &baselineOps();
const BackendOps *optimizingOps();

Builder *createCompiler(const BackendOps &ops);
void freeCompiler(Builder *b);
void *generateCode(Builder *b);
void freeMachine(const BackendOps &ops, void *machine);

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

void optimize(Builder &b);

} // namespace BytecodeJitLir

#endif
