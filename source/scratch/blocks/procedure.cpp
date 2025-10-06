#include "procedure.hpp"
#include "blockExecutor.hpp"
#include "interpret.hpp"
#include "sprite.hpp"
#include "unzip.hpp"
#include "value.hpp"

Value ProcedureBlocks::stringNumber(Block &block, Sprite *sprite) {
    const std::string name = Scratch::getFieldValue(block, "VALUE");
    if (name == "Scratch Everywhere! platform") {
        return Value(OS::getPlatform());
    }
    if (name == "\u200B\u200Breceived data\u200B\u200B") {
        return Scratch::dataNextProject;
    }
    return BlockExecutor::getCustomBlockValue(name, sprite, block);
}

Value ProcedureBlocks::booleanArgument(Block &block, Sprite *sprite) {
    const std::string name = Scratch::getFieldValue(block, "VALUE");
    if (name == "is Scratch Everywhere!?") return Value(true);
    if (name == "is New 3DS?") {
        return Value(OS::isNew3DS());
    }

    Value value = BlockExecutor::getCustomBlockValue(name, sprite, block);
    return Value(value.asInt() == 1);
}

BlockResult ProcedureBlocks::call(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {

    if (block.repeatTimes != -1 && !fromRepeat) {
        block.repeatTimes = -1;
    }

    if (block.repeatTimes == -1) {
        block.repeatTimes = -8;
        block.customBlockExecuted = false;

        // Run the custom block for the first time
        if (BlockExecutor::runCustomBlock(sprite, block, &block, withoutScreenRefresh) == BlockResult::RETURN) return BlockResult::RETURN;
        block.customBlockExecuted = true;

        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    // Check if any repeat blocks are still running inside the custom block
    if (block.customBlockPtr != nullptr &&
        !BlockExecutor::hasActiveRepeats(sprite, block.customBlockPtr->blockChainID)) {

        // std::cout << "done with custom!" << std::endl;

        // Custom block execution is complete
        block.repeatTimes = -1; // Reset for next use
        block.customBlockExecuted = false;
        block.customBlockPtr = nullptr;

        BlockExecutor::removeFromRepeatQueue(sprite, &block);

        return BlockResult::CONTINUE;
    }
    if (block.customBlockPtr == nullptr) {
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    return BlockResult::RETURN;
}

BlockResult ProcedureBlocks::definition(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}
