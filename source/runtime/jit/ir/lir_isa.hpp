#pragma once

#include <cstddef>
#include <cstdint>

namespace BytecodeJitLir {

using Word = intptr_t;
using UWord = uintptr_t;
using Int = int32_t;

using Opnd = Int;
inline constexpr Opnd NONE = 0;
inline constexpr Opnd R0 = 1, R1 = 2, R2 = 3, R3 = 4, R4 = 5, R5 = 6;
inline constexpr Opnd S0 = 11, S1 = 12, S2 = 13, S3 = 14, S4 = 15, S5 = 16, S6 = 17, S7 = 18;
inline constexpr Opnd FR0 = 33, FR1 = 34, FR2 = 35, FR3 = 36, FR4 = 37, FR5 = 38;
constexpr Opnd FR(int index) { return FR0 + index; }
inline constexpr Opnd MEM_FLAG = 1 << 12, MEM2_FLAG = 1 << 13, IMM = 1 << 14;
constexpr Opnd MEM0() { return MEM_FLAG; }
constexpr Opnd MEM1(Opnd base) { return MEM_FLAG | base; }
constexpr Opnd MEM2(Opnd base, Opnd index) { return MEM2_FLAG | base | (index << 6); }
inline constexpr Word WORD_SHIFT = sizeof(Word) == 8 ? 3 : 2;
constexpr bool isRegister(Opnd o) { return o != NONE && (o & (MEM_FLAG | MEM2_FLAG | IMM)) == 0; }
constexpr Opnd memBase(Opnd o) { return o & 63; }
constexpr Opnd memIndex(Opnd o) { return (o >> 6) & 63; }

using Op = Int;
enum : Op {
    MOV = 1,
    MOV_U8,
    MOV_U16,
    MOV_U32,
    MOV_S32,
    MOV32,
    ADD,
    SUB,
    MUL,
    AND,
    OR,
    XOR,
    SHL,
    LSHR,
    ASHR,
    LMUL_UW,
    DIVMOD_S32,
    MOV_F64,
    ADD_F64,
    SUB_F64,
    MUL_F64,
    DIV_F64,
    ABS_F64,
    CMP_F64,
    CONV_F64_FROM_SW,
    CONV_F64_FROM_S32,
    CONV_SW_FROM_F64,
    CONV_S32_FROM_F64,
};
inline constexpr Op OP32 = 1 << 8;
inline constexpr Op SET_Z = 1 << 9, SET_OVERFLOW = 1 << 10, SET_SIG_LESS = 1 << 11;
inline constexpr Op SET_F_LESS = 1 << 12, SET_F_GREATER = 1 << 13, SET_F_EQUAL = 1 << 14;
inline constexpr Op SUB32 = SUB | OP32, XOR32 = XOR | OP32, ASHR32 = ASHR | OP32;
constexpr Op baseOp(Op op) { return op & 0xff; }

using Cond = Int;
enum : Cond {
    JUMP = 1,
    CALL,
    EQUAL,
    NOT_EQUAL,
    NOT_ZERO,
    LESS_EQUAL,
    GREATER,
    GREATER_EQUAL,
    SIG_LESS,
    SIG_LESS_EQUAL,
    SIG_GREATER,
    SIG_GREATER_EQUAL,
    OVERFLOW_SET,
    UNORDERED,
    ORDERED_EQUAL,
    ORDERED_NOT_EQUAL,
    ORDERED_LESS,
    ORDERED_GREATER,
    ORDERED_LESS_EQUAL,
    ORDERED_GREATER_EQUAL,
    UNORDERED_OR_NOT_EQUAL,
    UNORDERED_OR_GREATER_EQUAL,
    F_EQUAL,
    F_LESS,
    F_GREATER,
};

using Sig = Int;
enum class ArgType : Int { Void,
                           W,
                           P,
                           F64,
                           I32 };
constexpr Sig sig(ArgType ret, ArgType a1 = ArgType::Void, ArgType a2 = ArgType::Void, ArgType a3 = ArgType::Void, ArgType a4 = ArgType::Void) {
    return static_cast<Int>(ret) | (static_cast<Int>(a1) << 4) | (static_cast<Int>(a2) << 8) | (static_cast<Int>(a3) << 12) | (static_cast<Int>(a4) << 16);
}
constexpr ArgType sigReturn(Sig s) { return static_cast<ArgType>(s & 15); }
constexpr ArgType sigArgument(Sig s, int index) { return static_cast<ArgType>((s >> (4 + 4 * index)) & 15); }

} // namespace BytecodeJitLir

#define LIR_ARGS1(r, a) ::BytecodeJitLir::sig(::BytecodeJitLir::ArgType::r, ::BytecodeJitLir::ArgType::a)
#define LIR_ARGS2(r, a, b) ::BytecodeJitLir::sig(::BytecodeJitLir::ArgType::r, ::BytecodeJitLir::ArgType::a, ::BytecodeJitLir::ArgType::b)
#define LIR_ARGS3(r, a, b, c) ::BytecodeJitLir::sig(::BytecodeJitLir::ArgType::r, ::BytecodeJitLir::ArgType::a, ::BytecodeJitLir::ArgType::b, ::BytecodeJitLir::ArgType::c)
#define LIR_ARGS1V(a) ::BytecodeJitLir::sig(::BytecodeJitLir::ArgType::Void, ::BytecodeJitLir::ArgType::a)
#define LIR_ARGS2V(a, b) ::BytecodeJitLir::sig(::BytecodeJitLir::ArgType::Void, ::BytecodeJitLir::ArgType::a, ::BytecodeJitLir::ArgType::b)
#define LIR_ARGS3V(a, b, c) ::BytecodeJitLir::sig(::BytecodeJitLir::ArgType::Void, ::BytecodeJitLir::ArgType::a, ::BytecodeJitLir::ArgType::b, ::BytecodeJitLir::ArgType::c)
