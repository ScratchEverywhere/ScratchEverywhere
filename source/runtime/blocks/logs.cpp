#include "blockUtils.hpp"
#include "os.hpp"

SCRATCH_BLOCK(logs, log) {
    Value arg0;
    if (!Scratch::getInput(block, "arg0", thread, sprite, arg0)) return BlockResult::REPEAT;

    Log::log("[PROJECT] " + arg0.asString());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(logs, warn) {
    Value arg0;
    if (!Scratch::getInput(block, "arg0", thread, sprite, arg0)) return BlockResult::REPEAT;

    Log::logWarning("[PROJECT] " + arg0.asString());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(logs, error) {
    Value arg0;
    if (!Scratch::getInput(block, "arg0", thread, sprite, arg0)) return BlockResult::REPEAT;

    Log::logError("[PROJECT] " + arg0.asString());
    return BlockResult::CONTINUE;
}