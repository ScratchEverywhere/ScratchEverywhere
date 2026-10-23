#include "blockUtils.hpp"
#include <log.hpp>

SCRATCH_BLOCK(logs, log) {
    std::string arg0;
    if (!Scratch::getInputValueAs(block, "arg0", thread, sprite, arg0)) return BlockResult::REPEAT;

    Log::log("[PROJECT] " + arg0);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(logs, warn) {
    std::string arg0;
    if (!Scratch::getInputValueAs(block, "arg0", thread, sprite, arg0)) return BlockResult::REPEAT;

    Log::logWarning("[PROJECT] " + arg0);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(logs, error) {
    std::string arg0;
    if (!Scratch::getInputValueAs(block, "arg0", thread, sprite, arg0)) return BlockResult::REPEAT;

    Log::logError("[PROJECT] " + arg0);
    return BlockResult::CONTINUE;
}
