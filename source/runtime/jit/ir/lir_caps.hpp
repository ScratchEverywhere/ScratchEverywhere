#pragma once

#include "lir_isa.hpp"

#if defined(JIT_BACKEND_SLJIT)
#include "../backends/sljit/caps.hpp"
#elif defined(JIT_BACKEND_ASMJIT)
#include "../backends/asmjit/caps.hpp"
#elif defined(JIT_BACKEND_LLVM)
#include "../backends/llvm/caps.hpp"
#else
#error "no JIT backend was selected (SE_JIT_BACKEND)"
#endif
