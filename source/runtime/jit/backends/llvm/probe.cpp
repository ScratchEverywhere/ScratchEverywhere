#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "jit_probe.hpp"

namespace BytecodeJitInternal {

bool backendUsable() { return true; }

} // namespace BytecodeJitInternal

#endif
