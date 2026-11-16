#pragma once

#include <sljitLir.h>

namespace BytecodeJitInternal {

inline constexpr bool wide = sizeof(sljit_sw) == 8;
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
inline constexpr bool bigEndian = true;
#else
inline constexpr bool bigEndian = false;
#endif
constexpr sljit_sw lowWord(sljit_sw offset) { return !wide && bigEndian ? offset + 4 : offset; }
constexpr sljit_sw highWord(sljit_sw offset) { return !wide && bigEndian ? offset : offset + 4; }

} // namespace BytecodeJitInternal
