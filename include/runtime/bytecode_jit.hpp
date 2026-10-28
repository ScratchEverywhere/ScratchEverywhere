#pragma once
#include "bytecode.hpp"
#include <cstdint>
#include <memory>
#include <se_export.hpp>
#include <vector>

#ifdef SE_JIT

SE_EXPORT double bytecodeMathOpApply(BytecodeMathOp op, double value);

namespace BytecodeJit {

enum class Result : uint32_t {
    Interpret = 0,
    Yield = 1,
};

SE_EXPORT bool enabled();

SE_EXPORT void compile(const BytecodeChunk &chunk, const BytecodeLinkTable &link);

SE_EXPORT Result run(const BytecodeChunk &chunk, BytecodeThread &thread, uint32_t entryId, Timer &sliceTimer, bool &sliceTimerStarted);

} // namespace BytecodeJit

#endif
