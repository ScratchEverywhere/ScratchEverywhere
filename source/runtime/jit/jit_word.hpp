#pragma once

#include "ir/lir_isa.hpp"

namespace BytecodeJitInternal {

namespace lir = BytecodeJitLir;

inline constexpr bool wide = sizeof(lir::Word) == 8;
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
inline constexpr bool bigEndian = true;
#else
inline constexpr bool bigEndian = false;
#endif
constexpr lir::Word lowWord(lir::Word offset) { return !wide && bigEndian ? offset + 4 : offset; }
constexpr lir::Word highWord(lir::Word offset) { return !wide && bigEndian ? offset : offset + 4; }

} // namespace BytecodeJitInternal
