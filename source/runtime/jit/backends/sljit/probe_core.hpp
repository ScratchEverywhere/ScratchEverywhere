#pragma once

#include "jit_word.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sljitLir.h>

namespace BytecodeJitInternal {

inline constexpr int wordChecks = 32;
inline constexpr int doubleChecks = 8;

struct Results {
    sljit_sw words[wordChecks];
    double doubles[doubleChecks];
    int64_t cells[2];
};

inline double SLJIT_FUNC probeCallee(double value, sljit_sw count) { return value * static_cast<double>(count) + 0.5; }

class ProbeProgram {
  public:
    explicit ProbeProgram(sljit_compiler *compiler) : c_(compiler) {}

    void build() {
        sljit_emit_enter(c_, 0, SLJIT_ARGS1(W, P), 5 | SLJIT_ENTER_FLOAT(4), 1, 0);

        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -5);
        sljit_emit_op2(c_, SLJIT_ASHR32, SLJIT_R2, 0, SLJIT_R0, 0, SLJIT_IMM, 31);
        sljit_emit_op2(c_, SLJIT_XOR32, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_R2, 0);
        sljit_emit_op2(c_, SLJIT_SUB32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R2, 0);
        sljit_emit_op1(c_, SLJIT_MOV_U32, SLJIT_R1, 0, SLJIT_R1, 0);
        word(0, SLJIT_R1);

        convert(1, 2.9, SLJIT_CONV_S32_FROM_F64, true);
        convert(2, -2.9, SLJIT_CONV_S32_FROM_F64, true);
        convert(3, 123456.75, SLJIT_CONV_SW_FROM_F64, false);

        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, -77);
        sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_SW, SLJIT_FR0, 0, SLJIT_R0, 0);
        number(0, SLJIT_FR0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 9);
        sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_S32, SLJIT_FR0, 0, SLJIT_R0, 0);
        number(1, SLJIT_FR0);
        sljit_emit_fset64(c_, SLJIT_FR0, 3.25);
        number(2, SLJIT_FR0);
        sljit_emit_fset64(c_, SLJIT_FR1, -2.5);
        sljit_emit_fop1(c_, SLJIT_ABS_F64, SLJIT_FR0, 0, SLJIT_FR1, 0);
        number(3, SLJIT_FR0);

        const double nan = std::numeric_limits<double>::quiet_NaN();
        compare(4, SLJIT_UNORDERED, nan, 1.0);
        compare(5, SLJIT_UNORDERED, 1.0, 2.0);
        compare(6, SLJIT_UNORDERED_OR_NOT_EQUAL, nan, nan);
        compare(7, SLJIT_ORDERED_EQUAL, 1.0, 1.0);
        compare(8, SLJIT_ORDERED_EQUAL, nan, nan);
        compare(9, SLJIT_ORDERED_LESS, 1.0, 2.0);
        compare(10, SLJIT_ORDERED_GREATER_EQUAL, 2.0, 1.0);
        compare(11, SLJIT_UNORDERED_OR_GREATER_EQUAL, nan, 1.0);
        compare(12, SLJIT_ORDERED_GREATER_EQUAL, nan, 1.0);
        compare(13, SLJIT_ORDERED_LESS_EQUAL, 1.0, 1.0);

        overflow(14, std::numeric_limits<sljit_sw>::min());
        overflow(15, 5);
        branch(16, -1, SLJIT_GREATER, 5);
        branch(17, -1, SLJIT_SIG_LESS_EQUAL, 0);
        branch(18, 5, SLJIT_SIG_GREATER, 0);

        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, static_cast<sljit_sw>(static_cast<sljit_uw>(1) << (sizeof(sljit_sw) * 8 - 1)));
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 4);
        sljit_emit_op0(c_, SLJIT_LMUL_UW);
        word(19, SLJIT_R0);
        word(20, SLJIT_R1);

        if (wide) {
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, static_cast<sljit_sw>(0x123456789ABCDEF0ll));
            word(21, SLJIT_R0);
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1);
            sljit_emit_op2(c_, SLJIT_SHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 32);
            word(22, SLJIT_R0);
            sljit_emit_op2(c_, SLJIT_LSHR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 32);
            word(23, SLJIT_R0);
        }

        sljit_emit_fset64(c_, SLJIT_FR0, 1.5);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 4);
        sljit_emit_icall(c_, SLJIT_CALL, SLJIT_ARGS2(F64, F64, W), SLJIT_IMM, reinterpret_cast<sljit_sw>(&probeCallee));
        number(4, SLJIT_FR0);

        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), lowWord(cellOffset(0)), SLJIT_IMM, 1);
        if (!wide) sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), highWord(cellOffset(0)), SLJIT_IMM, 0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), lowWord(cellOffset(1)));
        word(24, SLJIT_R0);

        sljit_emit_return(c_, SLJIT_MOV, SLJIT_IMM, 0);
    }

  private:
    sljit_compiler *c_;

    static sljit_sw wordOffset(int index) { return static_cast<sljit_sw>(offsetof(Results, words) + static_cast<size_t>(index) * sizeof(sljit_sw)); }
    static sljit_sw cellOffset(int index) { return static_cast<sljit_sw>(offsetof(Results, cells) + static_cast<size_t>(index) * sizeof(int64_t)); }
    static sljit_sw numberOffset(int index) { return static_cast<sljit_sw>(offsetof(Results, doubles) + static_cast<size_t>(index) * sizeof(double)); }

    void word(int index, sljit_s32 reg) { sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), wordOffset(index), reg, 0); }
    void number(int index, sljit_s32 reg) { sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), numberOffset(index), reg, 0); }

    void convert(int index, double value, sljit_s32 op, bool narrow) {
        sljit_emit_fset64(c_, SLJIT_FR0, value);
        sljit_emit_fop1(c_, op, SLJIT_R0, 0, SLJIT_FR0, 0);
        if (narrow) sljit_emit_op1(c_, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_R0, 0);
        word(index, SLJIT_R0);
    }

    template <typename Jump>
    void flag(int index, Jump emitJump) {
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), wordOffset(index), SLJIT_IMM, 0);
        sljit_jump *taken = emitJump();
        sljit_jump *end = sljit_emit_jump(c_, SLJIT_JUMP);
        sljit_set_label(taken, sljit_emit_label(c_));
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), wordOffset(index), SLJIT_IMM, 1);
        sljit_set_label(end, sljit_emit_label(c_));
    }

    void compare(int index, sljit_s32 type, double a, double b) {
        sljit_emit_fset64(c_, SLJIT_FR0, a);
        sljit_emit_fset64(c_, SLJIT_FR1, b);
        flag(index, [&] { return sljit_emit_fcmp(c_, type, SLJIT_FR0, 0, SLJIT_FR1, 0); });
    }

    void overflow(int index, sljit_sw value) {
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, value);
        flag(index, [&] {
            sljit_emit_op2u(c_, SLJIT_SUB | SLJIT_SET_OVERFLOW, SLJIT_R0, 0, SLJIT_IMM, 1);
            return sljit_emit_jump(c_, SLJIT_OVERFLOW);
        });
    }

    void branch(int index, sljit_sw value, sljit_s32 type, sljit_sw against) {
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, value);
        flag(index, [&] { return sljit_emit_cmp(c_, type, SLJIT_R0, 0, SLJIT_IMM, against); });
    }
};

inline bool probeBackend(void (*report)(const char *message, const char *detail)) {
    const auto check = [report](const char *what, bool ok) {
        if (!ok) report("backend check failed", what);
        return ok;
    };

    if (!sljit_has_cpu_feature(SLJIT_HAS_FPU)) {
        report("this CPU has no floating point unit the JIT can use", nullptr);
        return false;
    }

    sljit_compiler *compiler = sljit_create_compiler(nullptr);
    if (compiler == nullptr) return false;
    ProbeProgram(compiler).build();
    void *machine = sljit_generate_code(compiler, 0, nullptr);
    sljit_free_compiler(compiler);
    if (machine == nullptr) {
#ifdef SE_SLJIT_CUSTOM_EXEC
        report("could not allocate executable memory", se_sljit_exec_failure());
#else
        report("could not allocate executable memory", nullptr);
#endif
        return false;
    }

    Results results = {};
    results.cells[0] = static_cast<int64_t>(0x7e57ab1e5eed1234ll);
    results.cells[1] = 7;
    reinterpret_cast<sljit_sw (*)(Results *)>(machine)(&results);
    sljit_free_code(machine, nullptr);

    const sljit_sw *w = results.words;
    const double *d = results.doubles;
    bool ok = true;
    ok &= check("32-bit absolute value", w[0] == 5);
    ok &= check("truncating conversion to 32-bit integer", w[1] == 2 && w[2] == -2);
    ok &= check("truncating conversion to word", w[3] == 123456);
    ok &= check("integer to float conversion", d[0] == -77.0 && d[1] == 9.0);
    ok &= check("float constants and absolute value", d[2] == 3.25 && d[3] == 2.5);
    ok &= check("calling a function with a float argument and result", d[4] == 6.5);
    ok &= check("unordered comparison", w[4] == 1 && w[5] == 0 && w[6] == 1);
    ok &= check("ordered comparison", w[7] == 1 && w[8] == 0 && w[9] == 1 && w[10] == 1 && w[13] == 1);
    ok &= check("ordered comparison with NaN", w[11] == 1 && w[12] == 0);
    ok &= check("overflow flag", w[14] == 1 && w[15] == 0);
    ok &= check("integer branches", w[16] == 1 && w[17] == 1 && w[18] == 1);
    ok &= check("double width multiplication", w[19] == 0 && w[20] == 2);
    ok &= check("64-bit cells accessed by words", results.cells[0] == 1 && w[24] == 7);
    if (wide) ok &= check("64-bit immediates and shifts", w[21] == static_cast<sljit_sw>(0x123456789ABCDEF0ll) && w[22] == (static_cast<sljit_sw>(1) << 32) && w[23] == 1);
    return ok;
}

} // namespace BytecodeJitInternal
