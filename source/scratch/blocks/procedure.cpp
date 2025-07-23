#include "procedure.hpp"

Value ProcedureBlocks::stringNumber(Block &block, Sprite *sprite) {
    return BlockExecutor::getCustomBlockValue(block.fields.at("VALUE")[0], sprite, block);
}

Value ProcedureBlocks::booleanArgument(Block &block, Sprite *sprite) {
    Value value = BlockExecutor::getCustomBlockValue(block.fields.at("VALUE")[0], sprite, block);
    return Value(value.asInt() == 1);
}

BlockResult ProcedureBlocks::call(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh) {

    if (block.repeatTimes == -1) {
        block.repeatTimes = -8;
        block.customBlockExecuted = false;

        // Run the custom block for the first time
        BlockExecutor::runCustomBlock(sprite, block, &block, withoutScreenRefresh);
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

    return BlockResult::RETURN;
}

BlockResult ProcedureBlocks::definition(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh) {
    return BlockResult::CONTINUE;
}