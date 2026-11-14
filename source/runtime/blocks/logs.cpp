#include "blockUtils.hpp"
#include <log.hpp>
#ifdef SE_BYTECODE_STATS
#include <bytecode_executor.hpp>
#endif

SCRATCH_BLOCK(logs, log) {
    std::string arg0;
    if (!Scratch::getInputValueAs(block, "arg0", thread, sprite, arg0)) return BlockResult::REPEAT;

#ifdef SE_BYTECODE_STATS
    arg0 += BytecodeExecutor::takeStatsReport();
#endif
    Log::log("[PROJECT] " + arg0);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(logs, log, {"arg0", BytecodeStackKind::String});

SCRATCH_BLOCK(logs, warn) {
    std::string arg0;
    if (!Scratch::getInputValueAs(block, "arg0", thread, sprite, arg0)) return BlockResult::REPEAT;

    Log::logWarning("[PROJECT] " + arg0);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(logs, warn, {"arg0", BytecodeStackKind::String});

SCRATCH_BLOCK(logs, error) {
    std::string arg0;
    if (!Scratch::getInputValueAs(block, "arg0", thread, sprite, arg0)) return BlockResult::REPEAT;

    Log::logError("[PROJECT] " + arg0);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(logs, error, {"arg0", BytecodeStackKind::String});
