#pragma once

#include <cstdint>

#define LIR_FUNC

namespace BytecodeJitLir {

inline constexpr int floatRegisters = 15;
inline constexpr int savedRegisters = 8;
inline constexpr bool floatToWordOverflowsToMinimum = true;

} // namespace BytecodeJitLir
