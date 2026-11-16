#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "lir.hpp"
#include <asmjit/x86.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <unordered_map>
#include <vector>

namespace BytecodeJitLir {

namespace {

namespace x86 = asmjit::x86;

asmjit::JitRuntime &runtime() {
    static asmjit::JitRuntime instance;
    return instance;
}

const x86::Gp &gp(Opnd o) {
    static const x86::Gp table[16] = {x86::rdi, x86::rdi, x86::rsi, x86::rdx, x86::rcx, x86::r8, x86::r9, x86::rdi, x86::rdi, x86::rdi, x86::rdi, x86::rbx, x86::rbp, x86::r12, x86::r13, x86::r14};
    static const x86::Gp saved5 = x86::r15;
    if (o == S5) return saved5;
    return table[o < 16 ? o : 0];
}

class Translator {
  public:
    Translator(Builder &b, asmjit::x86::Assembler &a) : b_(b), a_(a) {}

    bool run() {
        for (const Instr &i : b_.code) {
            if (i.kind == Kind::SetLabel) targets_[i.jump] = i.label;
        }
        for (const Instr &i : b_.code) {
            if (failed_) return false;
            emit(i);
        }
        return !failed_;
    }

    asmjit::Label labelOf(const Label *label) {
        auto found = labels_.find(label);
        if (found != labels_.end()) return found->second;
        asmjit::Label created = a_.new_label();
        labels_[label] = created;
        return created;
    }

  private:
    Builder &b_;
    asmjit::x86::Assembler &a_;
    std::unordered_map<const Label *, asmjit::Label> labels_;
    std::unordered_map<const Jump *, const Label *> targets_;
    bool failed_ = false;

    void fail(const char *why) {
        if (std::getenv("SE_JIT_ASMJIT_LOG") != nullptr) std::fprintf(stderr, "[jit asmjit] cannot translate: %s\n", why);
        failed_ = true;
    }

    static bool isMemory(Opnd o) { return (o & (MEM_FLAG | MEM2_FLAG)) != 0; }
    static bool isImmediate(Opnd o) { return (o & IMM) != 0; }
    static bool isFloat(Opnd o) { return o >= FR0 && o < FR0 + 15; }
    static bool fits32(Word w) { return w >= INT32_MIN && w <= INT32_MAX; }

    static x86::Vec fr(Opnd o) { return x86::xmm(static_cast<uint32_t>(o - FR0)); }

    bool isGpr(Opnd o) const { return (o >= R0 && o <= R5) || (o >= S0 && o <= S5); }

    x86::Gp reg(Opnd o, int bits) {
        if (!isGpr(o)) {
            fail("bad register");
            return x86::rax;
        }
        const x86::Gp &r = gp(o);
        switch (bits) {
        case 8:
            return r.r8();
        case 16:
            return r.r16();
        case 32:
            return r.r32();
        default:
            return r.r64();
        }
    }

    x86::Mem memory(Opnd o, Word w, uint32_t size) {
        if ((o & MEM2_FLAG) != 0) {
            const Opnd base = memBase(o), index = memIndex(o);
            if (!isGpr(base) || !isGpr(index)) fail("bad indexed operand");
            return x86::ptr(gp(base).r64(), gp(index).r64(), static_cast<uint32_t>(w), 0, size);
        }
        const Opnd base = memBase(o);
        if (base == 0) {
            a_.mov(x86::r11, static_cast<int64_t>(w));
            return x86::ptr(x86::r11, 0, size);
        }
        if (!isGpr(base) || !fits32(w)) fail("bad memory operand");
        return x86::ptr(gp(base).r64(), static_cast<int32_t>(w), size);
    }

    static uint32_t moveSize(Op op) {
        switch (baseOp(op)) {
        case MOV_U8:
            return 1;
        case MOV_U16:
            return 2;
        case MOV_U32:
        case MOV_S32:
        case MOV32:
            return 4;
        default:
            return 8;
        }
    }

    static int64_t narrowConstant(Op op, Word w) {
        switch (baseOp(op)) {
        case MOV_U8:
            return static_cast<uint8_t>(w);
        case MOV_U16:
            return static_cast<uint16_t>(w);
        case MOV_U32:
        case MOV32:
            return static_cast<uint32_t>(w);
        case MOV_S32:
            return static_cast<int32_t>(w);
        default:
            return w;
        }
    }

    void loadMove(const x86::Gp &dst, Op op, Opnd src, Word w) {
        if (isImmediate(src)) {
            a_.mov(dst.r64(), narrowConstant(op, w));
            return;
        }
        if (isMemory(src)) {
            const uint32_t size = moveSize(op);
            const x86::Mem m = memory(src, w, size);
            switch (baseOp(op)) {
            case MOV_U8:
                a_.movzx(dst.r32(), m);
                break;
            case MOV_U16:
                a_.movzx(dst.r32(), m);
                break;
            case MOV_U32:
            case MOV32:
                a_.mov(dst.r32(), m);
                break;
            case MOV_S32:
                a_.movsxd(dst.r64(), m);
                break;
            default:
                a_.mov(dst.r64(), m);
                break;
            }
            return;
        }
        const x86::Gp source = reg(src, 64);
        switch (baseOp(op)) {
        case MOV_U8:
            a_.movzx(dst.r32(), source.r8());
            break;
        case MOV_U16:
            a_.movzx(dst.r32(), source.r16());
            break;
        case MOV_U32:
        case MOV32:
            a_.mov(dst.r32(), source.r32());
            break;
        case MOV_S32:
            a_.movsxd(dst.r64(), source.r32());
            break;
        default:
            if (dst.id() != source.id()) a_.mov(dst.r64(), source);
            break;
        }
    }

    void storeMove(Op op, Opnd dst, Word dstw, const x86::Gp &value) {
        const uint32_t size = moveSize(op);
        const x86::Mem m = memory(dst, dstw, size);
        switch (size) {
        case 1:
            a_.mov(m, value.r8());
            break;
        case 2:
            a_.mov(m, value.r16());
            break;
        case 4:
            a_.mov(m, value.r32());
            break;
        default:
            a_.mov(m, value.r64());
            break;
        }
    }

    void emitMove(const Instr &i) {
        if (isMemory(i.dst)) {
            if (isImmediate(i.src1)) {
                const uint32_t size = moveSize(i.op);
                const int64_t value = narrowConstant(i.op, i.src1w);
                const x86::Mem m = memory(i.dst, i.dstw, size);
                if (size == 8 && !fits32(value)) {
                    a_.mov(x86::r10, value);
                    a_.mov(m, x86::r10);
                } else {
                    a_.mov(m, asmjit::imm(static_cast<int32_t>(size == 4 ? static_cast<int32_t>(value) : value)));
                }
                return;
            }
            if (isMemory(i.src1)) {
                loadMove(x86::r10, i.op, i.src1, i.src1w);
                storeMove(i.op, i.dst, i.dstw, x86::r10);
                return;
            }
            storeMove(i.op, i.dst, i.dstw, reg(i.src1, 64));
            return;
        }
        loadMove(reg(i.dst, 64), i.op, i.src1, i.src1w);
    }

    x86::Gp intoRegister(Opnd o, Word w, bool narrow, const x86::Gp &scratch) {
        if (isGpr(o)) return reg(o, narrow ? 32 : 64);
        if (isImmediate(o)) {
            a_.mov(scratch.r64(), static_cast<int64_t>(narrow ? static_cast<int32_t>(w) : w));
            return narrow ? scratch.r32() : scratch.r64();
        }
        const x86::Mem m = memory(o, w, narrow ? 4 : 8);
        if (narrow) a_.mov(scratch.r32(), m);
        else a_.mov(scratch.r64(), m);
        return narrow ? scratch.r32() : scratch.r64();
    }

    void emitCompare(Opnd lhs, Word lhsw, Opnd rhs, Word rhsw, bool narrow) {
        x86::Gp left;
        if (isMemory(lhs) && !isMemory(rhs) && !isImmediate(rhs)) {
            const x86::Mem m = memory(lhs, lhsw, narrow ? 4 : 8);
            a_.cmp(m, reg(rhs, narrow ? 32 : 64));
            return;
        }
        if (isMemory(lhs) && isImmediate(rhs) && fits32(rhsw)) {
            const x86::Mem m = memory(lhs, lhsw, narrow ? 4 : 8);
            a_.cmp(m, asmjit::imm(static_cast<int32_t>(rhsw)));
            return;
        }
        left = intoRegister(lhs, lhsw, narrow, x86::r10);
        if (isGpr(rhs)) {
            a_.cmp(left, reg(rhs, narrow ? 32 : 64));
        } else if (isImmediate(rhs)) {
            if (fits32(rhsw)) {
                a_.cmp(left, asmjit::imm(static_cast<int32_t>(rhsw)));
            } else {
                a_.mov(x86::rax, static_cast<int64_t>(rhsw));
                a_.cmp(left, narrow ? x86::rax.r32() : x86::rax.r64());
            }
        } else {
            a_.cmp(left, memory(rhs, rhsw, narrow ? 4 : 8));
        }
    }

    void emitOp2(const Instr &i, bool store) {
        const bool narrow = (i.op & OP32) != 0;
        const Op base = baseOp(i.op);
        if (!store && base == SUB) {
            emitCompare(i.src1, i.src1w, i.src2, i.src2w, narrow);
            return;
        }
        x86::Gp work = x86::r10;
        if (store && isGpr(i.dst) && !(isGpr(i.src2) && i.src2 == i.dst && i.src1 != i.dst)) work = gp(i.dst);
        const x86::Gp w = narrow ? work.r32() : work.r64();
        if (isGpr(i.src1)) {
            if (gp(i.src1).id() != work.id() || false) a_.mov(w, reg(i.src1, narrow ? 32 : 64));
        } else if (isImmediate(i.src1)) {
            a_.mov(work.r64(), static_cast<int64_t>(narrow ? static_cast<int32_t>(i.src1w) : i.src1w));
        } else {
            const x86::Mem m = memory(i.src1, i.src1w, narrow ? 4 : 8);
            a_.mov(w, m);
        }
        if (base == SHL || base == LSHR || base == ASHR) {
            if (!isImmediate(i.src2)) {
                fail("variable shift");
                return;
            }
            const int amount = static_cast<int>(i.src2w) & (narrow ? 31 : 63);
            if (base == SHL) a_.shl(w, amount);
            else if (base == LSHR) a_.shr(w, amount);
            else a_.sar(w, amount);
        } else if (isImmediate(i.src2) && !fits32(i.src2w)) {
            a_.mov(x86::rax, static_cast<int64_t>(i.src2w));
            applyBinary(base, w, narrow ? x86::rax.r32() : x86::rax.r64());
        } else if (isImmediate(i.src2)) {
            const asmjit::Imm value(static_cast<int32_t>(i.src2w));
            switch (base) {
            case ADD:
                a_.add(w, value);
                break;
            case SUB:
                a_.sub(w, value);
                break;
            case MUL:
                a_.imul(w, w, value);
                break;
            case AND:
                a_.and_(w, value);
                break;
            case OR:
                a_.or_(w, value);
                break;
            case XOR:
                a_.xor_(w, value);
                break;
            default:
                fail("unsupported integer operation");
                break;
            }
        } else if (isGpr(i.src2)) {
            applyBinary(base, w, reg(i.src2, narrow ? 32 : 64));
        } else {
            const x86::Mem m = memory(i.src2, i.src2w, narrow ? 4 : 8);
            switch (base) {
            case ADD:
                a_.add(w, m);
                break;
            case SUB:
                a_.sub(w, m);
                break;
            case MUL:
                a_.imul(w, m);
                break;
            case AND:
                a_.and_(w, m);
                break;
            case OR:
                a_.or_(w, m);
                break;
            case XOR:
                a_.xor_(w, m);
                break;
            default:
                fail("unsupported integer operation");
                break;
            }
        }
        if (!store) return;
        if (isMemory(i.dst)) {
            const x86::Mem m = memory(i.dst, i.dstw, narrow ? 4 : 8);
            a_.mov(m, w);
        } else if (work.id() != gp(i.dst).id()) {
            a_.mov(reg(i.dst, narrow ? 32 : 64), w);
        }
    }

    void applyBinary(Op base, const x86::Gp &dst, const x86::Gp &src) {
        switch (base) {
        case ADD:
            a_.add(dst, src);
            break;
        case SUB:
            a_.sub(dst, src);
            break;
        case MUL:
            a_.imul(dst, src);
            break;
        case AND:
            a_.and_(dst, src);
            break;
        case OR:
            a_.or_(dst, src);
            break;
        case XOR:
            a_.xor_(dst, src);
            break;
        default:
            fail("unsupported integer operation");
            break;
        }
    }

    void jumpIf(Cond type, const asmjit::Label &target) {
        switch (type & 0xff) {
        case JUMP:
            a_.jmp(target);
            break;
        case EQUAL:
            a_.je(target);
            break;
        case NOT_EQUAL:
        case NOT_ZERO:
            a_.jne(target);
            break;
        case LESS_EQUAL:
            a_.jbe(target);
            break;
        case GREATER:
            a_.ja(target);
            break;
        case GREATER_EQUAL:
            a_.jae(target);
            break;
        case SIG_LESS:
            a_.jl(target);
            break;
        case SIG_LESS_EQUAL:
            a_.jle(target);
            break;
        case SIG_GREATER:
            a_.jg(target);
            break;
        case SIG_GREATER_EQUAL:
            a_.jge(target);
            break;
        case OVERFLOW_SET:
            a_.jo(target);
            break;
        case F_EQUAL:
            a_.je(target);
            break;
        case F_LESS:
            a_.jb(target);
            break;
        case F_GREATER:
            a_.ja(target);
            break;
        default:
            fail("unsupported condition");
            break;
        }
    }

    x86::Vec floatSource(Opnd o, Word w, const x86::Vec &scratch) {
        if (isFloat(o)) return fr(o);
        if (isMemory(o)) {
            a_.movsd(scratch, memory(o, w, 8));
            return scratch;
        }
        fail("bad float operand");
        return scratch;
    }

    void emitFloatCompare(Cond type, Opnd lhs, Word lhsw, Opnd rhs, Word rhsw, const asmjit::Label &target) {
        const x86::Vec a = floatSource(lhs, lhsw, x86::xmm15);
        asmjit::Label skip = a_.new_label();
        auto compare = [&](const x86::Vec &first, Opnd second, Word secondw) {
            if (isFloat(second)) a_.ucomisd(first, fr(second));
            else if (isMemory(second)) a_.ucomisd(first, memory(second, secondw, 8));
            else fail("bad float operand");
        };
        const x86::Vec b = floatSource(rhs, rhsw, x86::xmm14);
        (void)compare;
        switch (type & 0xff) {
        case UNORDERED:
            a_.ucomisd(a, b);
            a_.jp(target);
            break;
        case ORDERED_EQUAL:
            a_.ucomisd(a, b);
            a_.jp(skip);
            a_.je(target);
            break;
        case ORDERED_NOT_EQUAL:
            a_.ucomisd(a, b);
            a_.jp(skip);
            a_.jne(target);
            break;
        case UNORDERED_OR_NOT_EQUAL:
            a_.ucomisd(a, b);
            a_.jp(target);
            a_.jne(target);
            break;
        case ORDERED_LESS:
            a_.ucomisd(b, a);
            a_.ja(target);
            break;
        case ORDERED_GREATER:
            a_.ucomisd(a, b);
            a_.ja(target);
            break;
        case ORDERED_LESS_EQUAL:
            a_.ucomisd(b, a);
            a_.jae(target);
            break;
        case ORDERED_GREATER_EQUAL:
            a_.ucomisd(a, b);
            a_.jae(target);
            break;
        case UNORDERED_OR_GREATER_EQUAL:
            a_.ucomisd(a, b);
            a_.jp(target);
            a_.jae(target);
            break;
        default:
            fail("unsupported float condition");
            break;
        }
        a_.bind(skip);
    }

    void storeFloat(Opnd dst, Word dstw, const x86::Vec &value) {
        if (isFloat(dst)) {
            if (fr(dst).id() != value.id()) a_.movsd(fr(dst), value);
            return;
        }
        if (isMemory(dst)) {
            a_.movsd(memory(dst, dstw, 8), value);
            return;
        }
        fail("bad float destination");
    }

    void emitFloat1(const Instr &i) {
        switch (baseOp(i.op)) {
        case MOV_F64:
            if (isFloat(i.dst) && isMemory(i.src1)) a_.movsd(fr(i.dst), memory(i.src1, i.src1w, 8));
            else if (isFloat(i.dst) && isFloat(i.src1)) {
                if (i.dst != i.src1) a_.movaps(fr(i.dst), fr(i.src1));
            } else storeFloat(i.dst, i.dstw, floatSource(i.src1, i.src1w, x86::xmm15));
            break;
        case ABS_F64: {
            const x86::Vec source = floatSource(i.src1, i.src1w, x86::xmm15);
            a_.movq(x86::r10, source);
            a_.btr(x86::r10, 63);
            if (isFloat(i.dst)) a_.movq(fr(i.dst), x86::r10);
            else {
                a_.movq(x86::xmm15, x86::r10);
                storeFloat(i.dst, i.dstw, x86::xmm15);
            }
            break;
        }
        case CMP_F64: {
            const x86::Vec first = floatSource(i.dst, i.dstw, x86::xmm15);
            if (isFloat(i.src1)) a_.ucomisd(first, fr(i.src1));
            else a_.ucomisd(first, memory(i.src1, i.src1w, 8));
            break;
        }
        case CONV_F64_FROM_SW:
        case CONV_F64_FROM_S32: {
            const bool narrow = baseOp(i.op) == CONV_F64_FROM_S32;
            const x86::Vec out = isFloat(i.dst) ? fr(i.dst) : x86::xmm15;
            a_.xorps(out, out);
            if (isMemory(i.src1)) a_.cvtsi2sd(out, memory(i.src1, i.src1w, narrow ? 4 : 8));
            else a_.cvtsi2sd(out, intoRegister(i.src1, i.src1w, narrow, x86::r10));
            if (!isFloat(i.dst)) storeFloat(i.dst, i.dstw, out);
            break;
        }
        case CONV_SW_FROM_F64:
        case CONV_S32_FROM_F64: {
            const bool narrow = baseOp(i.op) == CONV_S32_FROM_F64;
            const x86::Vec source = floatSource(i.src1, i.src1w, x86::xmm15);
            const x86::Gp out = isGpr(i.dst) ? gp(i.dst) : x86::r10;
            a_.cvttsd2si(narrow ? out.r32() : out.r64(), source);
            if (!isGpr(i.dst)) a_.mov(memory(i.dst, i.dstw, narrow ? 4 : 8), narrow ? x86::r10.r32() : x86::r10);
            break;
        }
        default:
            fail("unsupported float conversion");
            break;
        }
    }

    void emitFloat2(const Instr &i) {
        const x86::Vec out = isFloat(i.dst) ? fr(i.dst) : x86::xmm15;
        const bool sameDestination = isFloat(i.dst) && isFloat(i.src2) && i.dst == i.src2 && i.src1 != i.dst;
        const x86::Vec work = sameDestination ? x86::xmm14 : out;
        const x86::Vec first = floatSource(i.src1, i.src1w, work);
        if (first.id() != work.id()) a_.movaps(work, first);
        auto apply = [&](auto instruction) {
            if (isFloat(i.src2)) (a_.*instruction)(work, fr(i.src2));
            else (a_.*instruction)(work, memory(i.src2, i.src2w, 8));
        };
        switch (baseOp(i.op)) {
        case ADD_F64:
            if (isFloat(i.src2)) a_.addsd(work, fr(i.src2));
            else a_.addsd(work, memory(i.src2, i.src2w, 8));
            break;
        case SUB_F64:
            if (isFloat(i.src2)) a_.subsd(work, fr(i.src2));
            else a_.subsd(work, memory(i.src2, i.src2w, 8));
            break;
        case MUL_F64:
            if (isFloat(i.src2)) a_.mulsd(work, fr(i.src2));
            else a_.mulsd(work, memory(i.src2, i.src2w, 8));
            break;
        case DIV_F64:
            if (isFloat(i.src2)) a_.divsd(work, fr(i.src2));
            else a_.divsd(work, memory(i.src2, i.src2w, 8));
            break;
        default:
            fail("unsupported float operation");
            break;
        }
        (void)apply;
        if (isFloat(i.dst)) {
            if (work.id() != out.id()) a_.movaps(out, work);
        } else {
            storeFloat(i.dst, i.dstw, work);
        }
    }

    void emitCall(const Instr &i) {
        if (isImmediate(i.src1)) {
            a_.mov(x86::r11, static_cast<int64_t>(i.src1w));
        } else {
            a_.mov(x86::r11, intoRegister(i.src1, i.src1w, false, x86::r11));
        }
        int floats = 0;
        for (int index = 0; index < 4 && sigArgument(i.sig, index) != ArgType::Void; index++)
            if (sigArgument(i.sig, index) == ArgType::F64) floats++;
        a_.call(x86::r11);
        switch (sigReturn(i.sig)) {
        case ArgType::W:
        case ArgType::P:
            a_.mov(x86::rdi, x86::rax);
            break;
        case ArgType::I32:
            a_.mov(x86::edi, x86::eax);
            break;
        default:
            break;
        }
    }

    void epilogue() {
        a_.add(x86::rsp, 8);
        a_.pop(x86::r15);
        a_.pop(x86::r14);
        a_.pop(x86::r13);
        a_.pop(x86::r12);
        a_.pop(x86::rbx);
        a_.pop(x86::rbp);
        a_.ret();
    }

    void emit(const Instr &i) {
        switch (i.kind) {
        case Kind::Enter:
            a_.push(x86::rbp);
            a_.push(x86::rbx);
            a_.push(x86::r12);
            a_.push(x86::r13);
            a_.push(x86::r14);
            a_.push(x86::r15);
            a_.sub(x86::rsp, 8);
            a_.mov(x86::rbx, x86::rdi);
            a_.mov(x86::rbp, x86::rsi);
            break;
        case Kind::Return:
            loadMove(x86::rax, i.op, i.src1, i.src1w);
            epilogue();
            break;
        case Kind::Op0:
            if (baseOp(i.op) == DIVMOD_S32) {
                a_.mov(x86::r11, x86::rdx);
                a_.mov(x86::eax, x86::edi);
                a_.cdq(x86::edx, x86::eax);
                a_.idiv(x86::edx, x86::eax, x86::esi);
                a_.mov(x86::edi, x86::eax);
                a_.mov(x86::esi, x86::edx);
                a_.mov(x86::rdx, x86::r11);
            } else if (baseOp(i.op) == LMUL_UW) {
                a_.mov(x86::r11, x86::rdx);
                a_.mov(x86::rax, x86::rdi);
                a_.mul(x86::rdx, x86::rax, x86::rsi);
                a_.mov(x86::rdi, x86::rax);
                a_.mov(x86::rsi, x86::rdx);
                a_.mov(x86::rdx, x86::r11);
            } else {
                fail("unsupported op0");
            }
            break;
        case Kind::Op1:
            emitMove(i);
            break;
        case Kind::Op2:
            emitOp2(i, true);
            break;
        case Kind::Op2U:
            emitOp2(i, false);
            break;
        case Kind::OpFlags: {
            switch (i.counts[0] & 0xff) {
            case EQUAL:
            case F_EQUAL:
                a_.sete(x86::r10b);
                break;
            case NOT_EQUAL:
            case NOT_ZERO:
                a_.setne(x86::r10b);
                break;
            case F_LESS:
                a_.setb(x86::r10b);
                break;
            case F_GREATER:
                a_.seta(x86::r10b);
                break;
            case OVERFLOW_SET:
                a_.seto(x86::r10b);
                break;
            case SIG_LESS:
                a_.setl(x86::r10b);
                break;
            case SIG_GREATER_EQUAL:
                a_.setge(x86::r10b);
                break;
            default:
                fail("unsupported flag condition");
                break;
            }
            a_.movzx(x86::r10d, x86::r10b);
            if (isMemory(i.dst)) storeMove(i.op, i.dst, i.dstw, x86::r10);
            else a_.mov(reg(i.dst, 64), x86::r10);
            break;
        }
        case Kind::FOp1:
            emitFloat1(i);
            break;
        case Kind::FOp2:
            emitFloat2(i);
            break;
        case Kind::FSet64: {
            uint64_t bits;
            std::memcpy(&bits, &i.value, sizeof bits);
            if (bits == 0) {
                a_.xorps(fr(i.dst), fr(i.dst));
            } else {
                a_.mov(x86::r11, bits);
                a_.movq(fr(i.dst), x86::r11);
            }
            break;
        }
        case Kind::IJump:
            if ((i.src1 & MEM2_FLAG) == 0) {
                fail("indirect jump that is not a table dispatch");
                break;
            }
            a_.jmp(memory(i.src1, i.src1w, 8));
            break;
        case Kind::ICall:
            emitCall(i);
            break;
        case Kind::JumpOp: {
            const auto target = targets_.find(i.jump);
            if (target == targets_.end()) {
                fail("jump without a label");
                break;
            }
            jumpIf(i.op, labelOf(target->second));
            break;
        }
        case Kind::Cmp: {
            const auto target = targets_.find(i.jump);
            if (target == targets_.end()) {
                fail("jump without a label");
                break;
            }
            emitCompare(i.src1, i.src1w, i.src2, i.src2w, (i.op & OP32) != 0);
            jumpIf(i.op, labelOf(target->second));
            break;
        }
        case Kind::FCmp: {
            const auto target = targets_.find(i.jump);
            if (target == targets_.end()) {
                fail("jump without a label");
                break;
            }
            emitFloatCompare(i.op, i.src1, i.src1w, i.src2, i.src2w, labelOf(target->second));
            break;
        }
        case Kind::LabelOp:
            a_.bind(labelOf(i.label));
            break;
        case Kind::SetLabel:
            break;
        }
    }
};

struct Result {
    std::unordered_map<const Label *, uintptr_t> addresses;
};

} // namespace

void *asmjitGenerate(Builder &b) {
    asmjit::CodeHolder code;
    code.init(runtime().environment(), runtime().cpu_features());
    asmjit::x86::Assembler a(&code);
    Translator translator(b, a);
    if (!translator.run()) return nullptr;
    void *fn = nullptr;
    if (runtime().add(&fn, &code) != asmjit::Error::kOk) return nullptr;
    b.machineSize = code.code_size();
    for (Label &label : b.labels) {
        const asmjit::Label handle = translator.labelOf(&label);
        label.real = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(fn) + code.label_offset_from_base(handle));
    }
    return fn;
}

void asmjitRelease(Builder &) {}

void asmjitFreeMachine(void *machine) { runtime().release(machine); }

UWord asmjitLabelAddress(Label *label) { return reinterpret_cast<UWord>(label->real); }

bool asmjitUsable() { return true; }

const BackendOps asmjitOps = {"asmjit", &asmjitUsable, &asmjitGenerate, &asmjitRelease, &asmjitFreeMachine, &asmjitLabelAddress, false};

} // namespace BytecodeJitLir

#endif
