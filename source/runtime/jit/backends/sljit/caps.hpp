#pragma once

#include <sljitLir.h>

#define LIR_FUNC SLJIT_FUNC

namespace BytecodeJitLir {

inline constexpr int floatRegisters = SLJIT_NUMBER_OF_FLOAT_REGISTERS;
inline constexpr int savedRegisters = SLJIT_NUMBER_OF_SAVED_REGISTERS;
inline constexpr bool floatToWordOverflowsToMinimum =
#if (defined SLJIT_CONFIG_X86 && SLJIT_CONFIG_X86)
    true;
#else
    false;
#endif

} // namespace BytecodeJitLir
