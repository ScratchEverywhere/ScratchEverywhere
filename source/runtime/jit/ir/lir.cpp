#include "lir.hpp"

#ifdef SE_JIT

#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread.hpp>

namespace BytecodeJitLir {

namespace {

SE_Mutex &codegenMutex() {
    static SE_Mutex mutex;
    return mutex;
}

struct CodegenLock {
    CodegenLock() { codegenMutex().lock(); }
    ~CodegenLock() { codegenMutex().unlock(); }
};

} // namespace

void freeMachine(const BackendOps &ops, void *machine) {
    if (ops.needsLock) {
        CodegenLock lock;
        ops.freeMachine(machine);
    } else {
        ops.freeMachine(machine);
    }
}

const BackendOps &baselineOps() {
#if defined(JIT_BACKEND_SLJIT)
    return sljitOps;
#elif defined(JIT_BACKEND_ASMJIT)
    return asmjitOps;
#else
    return llvmOps;
#endif
}

const BackendOps *optimizingOps() {
#if defined(JIT_OPT_BACKEND_SLJIT) || defined(JIT_OPT_BACKEND_ASMJIT) || defined(JIT_OPT_BACKEND_LLVM)
    static const BackendOps *ops = [] () -> const BackendOps * {
        const char *off = std::getenv("SE_JIT_OPT");
        if (off != nullptr && std::string(off) == "0") return nullptr;
#if defined(JIT_OPT_BACKEND_SLJIT)
        const BackendOps &candidate = sljitOps;
#elif defined(JIT_OPT_BACKEND_ASMJIT)
        const BackendOps &candidate = asmjitOps;
#else
        const BackendOps &candidate = llvmOps;
#endif
        return candidate.usable() ? &candidate : nullptr;
    }();
    return ops;
#else
    return nullptr;
#endif
}

Builder *createCompiler(const BackendOps &ops) {
    Builder *b = new Builder();
    b->ops = &ops;
    return b;
}

Mark mark(const Builder *b) { return {b->code.size(), b->jumps.size(), b->labels.size()}; }

void rewind(Builder *b, const Mark &to) {
    b->code.resize(to.code);
    b->jumps.resize(to.jumps);
    b->labels.resize(to.labels);
}

void freeCompiler(Builder *b) {
    if (b == nullptr) return;
    b->ops->release(*b);
    delete b;
}

void *generateCode(Builder *b) {
    optimize(*b);
    void *machine;
    if (b->ops->needsLock) {
        CodegenLock lock;
        machine = b->ops->generate(*b);
    } else {
        machine = b->ops->generate(*b);
    }
    b->machine = machine;
    if (const char *dump = std::getenv("SE_LIR_DUMP"); dump != nullptr && machine != nullptr) {
        static int index = 0;
        const std::string name = std::string(dump) + "." + std::to_string(index++) + ".bin";
        if (FILE *out = std::fopen(name.c_str(), "wb")) {
            std::fwrite(machine, 1, b->machineSize, out);
            std::fclose(out);
        }
        if (FILE *base = std::fopen((std::string(dump) + "." + std::to_string(index - 1) + ".base").c_str(), "w")) {
            std::fprintf(base, "%lu\n", static_cast<unsigned long>(reinterpret_cast<uintptr_t>(machine)));
            std::fclose(base);
        }
    }
    return machine;
}

namespace {

Instr &record(Builder *b, Kind kind) {
    b->code.emplace_back();
    Instr &i = b->code.back();
    i.kind = kind;
    return i;
}

Jump *newJump(Builder *b) {
    b->jumps.emplace_back();
    Jump *j = &b->jumps.back();
    j->id = static_cast<int>(b->jumps.size());
    j->owner = b;
    return j;
}

} // namespace

Int emitEnter(Builder *b, Sig sig, Int scratchRegisters, Int savedRegisters, Int floatRegisters, Int localSize) {
    Instr &i = record(b, Kind::Enter);
    i.sig = sig;
    i.counts[0] = scratchRegisters;
    i.counts[1] = savedRegisters;
    i.counts[2] = floatRegisters;
    i.counts[3] = localSize;
    return 0;
}

Int emitReturn(Builder *b, Op op, Opnd src, Word srcw) {
    Instr &i = record(b, Kind::Return);
    i.op = op;
    i.src1 = src;
    i.src1w = srcw;
    return 0;
}

Int emitOp0(Builder *b, Op op) {
    record(b, Kind::Op0).op = op;
    return 0;
}

Int emitOp1(Builder *b, Op op, Opnd dst, Word dstw, Opnd src, Word srcw) {
    Instr &i = record(b, Kind::Op1);
    i.op = op;
    i.dst = dst;
    i.dstw = dstw;
    i.src1 = src;
    i.src1w = srcw;
    return 0;
}

Int emitOp2(Builder *b, Op op, Opnd dst, Word dstw, Opnd src1, Word src1w, Opnd src2, Word src2w) {
    Instr &i = record(b, Kind::Op2);
    i.op = op;
    i.dst = dst;
    i.dstw = dstw;
    i.src1 = src1;
    i.src1w = src1w;
    i.src2 = src2;
    i.src2w = src2w;
    return 0;
}

Int emitOp2U(Builder *b, Op op, Opnd src1, Word src1w, Opnd src2, Word src2w) {
    Instr &i = record(b, Kind::Op2U);
    i.op = op;
    i.src1 = src1;
    i.src1w = src1w;
    i.src2 = src2;
    i.src2w = src2w;
    return 0;
}

Int emitOpFlags(Builder *b, Op op, Opnd dst, Word dstw, Cond type) {
    Instr &i = record(b, Kind::OpFlags);
    i.op = op;
    i.dst = dst;
    i.dstw = dstw;
    i.counts[0] = type;
    return 0;
}

Int emitFOp1(Builder *b, Op op, Opnd dst, Word dstw, Opnd src, Word srcw) {
    Instr &i = record(b, Kind::FOp1);
    i.op = op;
    i.dst = dst;
    i.dstw = dstw;
    i.src1 = src;
    i.src1w = srcw;
    return 0;
}

Int emitFOp2(Builder *b, Op op, Opnd dst, Word dstw, Opnd src1, Word src1w, Opnd src2, Word src2w) {
    Instr &i = record(b, Kind::FOp2);
    i.op = op;
    i.dst = dst;
    i.dstw = dstw;
    i.src1 = src1;
    i.src1w = src1w;
    i.src2 = src2;
    i.src2w = src2w;
    return 0;
}

Int emitFSet64(Builder *b, Opnd freg, double value) {
    Instr &i = record(b, Kind::FSet64);
    i.dst = freg;
    i.value = value;
    return 0;
}

Int emitIJump(Builder *b, Cond type, Opnd src, Word srcw) {
    Instr &i = record(b, Kind::IJump);
    i.op = type;
    i.src1 = src;
    i.src1w = srcw;
    return 0;
}

Int emitICall(Builder *b, Cond type, Sig sig, Opnd src, Word srcw) {
    Instr &i = record(b, Kind::ICall);
    i.op = type;
    i.sig = sig;
    i.src1 = src;
    i.src1w = srcw;
    return 0;
}

Jump *emitJump(Builder *b, Cond type) {
    Jump *j = newJump(b);
    Instr &i = record(b, Kind::JumpOp);
    i.op = type;
    i.jump = j;
    return j;
}

Jump *emitCmp(Builder *b, Cond type, Opnd src1, Word src1w, Opnd src2, Word src2w) {
    Jump *j = newJump(b);
    Instr &i = record(b, Kind::Cmp);
    i.op = type;
    i.src1 = src1;
    i.src1w = src1w;
    i.src2 = src2;
    i.src2w = src2w;
    i.jump = j;
    return j;
}

Jump *emitFCmp(Builder *b, Cond type, Opnd src1, Word src1w, Opnd src2, Word src2w) {
    Jump *j = newJump(b);
    Instr &i = record(b, Kind::FCmp);
    i.op = type;
    i.src1 = src1;
    i.src1w = src1w;
    i.src2 = src2;
    i.src2w = src2w;
    i.jump = j;
    return j;
}

Label *emitLabel(Builder *b) {
    b->labels.emplace_back();
    Label *l = &b->labels.back();
    l->id = static_cast<int>(b->labels.size());
    record(b, Kind::LabelOp).label = l;
    return l;
}

void setLabel(Jump *jump, Label *label) {
    Instr &i = record(jump->owner, Kind::SetLabel);
    i.jump = jump;
    i.label = label;
}

} // namespace BytecodeJitLir

#endif
