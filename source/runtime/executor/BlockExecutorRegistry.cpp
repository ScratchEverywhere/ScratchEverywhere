#include "blockExecutor.hpp"

std::unordered_map<std::string, BlockFunc> &BlockExecutor::getHandlers() {
    static std::unordered_map<std::string, BlockFunc> handlers;
    return handlers;
}

std::unordered_map<std::string, std::vector<BytecodeInputSignature>> &BlockExecutor::getInputSignatures() {
    static std::unordered_map<std::string, std::vector<BytecodeInputSignature>> signatures;
    return signatures;
}

std::unordered_map<std::string, BytecodePositionalFn> &BlockExecutor::getPositionalHandlers() {
    static std::unordered_map<std::string, BytecodePositionalFn> handlers;
    return handlers;
}

std::unordered_set<std::string> &BlockExecutor::getBlockingBlocks() {
    static std::unordered_set<std::string> blocks;
    return blocks;
}
