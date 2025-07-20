#include "control.hpp"

BlockResult ControlBlocks::If(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    Value conditionValue = Scratch::getInputValue(block,"CONDITION",sprite);
    bool condition = false;
    if(conditionValue.isNumeric()){
        condition = conditionValue.asDouble() != 0.0;
    } else condition = !conditionValue.asString().empty();

    if(condition){
        auto it = block.parsedInputs.find("SUBSTACK");
        if(it != block.parsedInputs.end()){
            Block* subBlock = &sprite->blocks[it->second.blockId];
            if(subBlock){
                executor.runBlock(*subBlock,sprite);
            }
        }
    }
    return BlockResult::CONTINUE;
}

BlockResult ControlBlocks::ifElse(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    Value conditionValue = Scratch::getInputValue(block,"CONDITION",sprite);
    bool condition = false;
    if(conditionValue.isNumeric()){
        condition = conditionValue.asDouble() != 0.0;
    } else condition = !conditionValue.asString().empty();

    if(condition){
        auto it = block.parsedInputs.find("SUBSTACK");
        if(it != block.parsedInputs.end()){
            Block* subBlock = &sprite->blocks[it->second.blockId];
            if(subBlock){
                executor.runBlock(*subBlock,sprite);
            }
        }
    } else{
        auto it = block.parsedInputs.find("SUBSTACK2");
        if(it != block.parsedInputs.end()){
            Block* subBlock = &sprite->blocks[it->second.blockId];
            if(subBlock){
                executor.runBlock(*subBlock,sprite);
            }
        }
    }
    return BlockResult::CONTINUE;
}

BlockResult ControlBlocks::createCloneOf(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    //std::cout << "Trying " << std::endl;

    Block* cloneOptions = nullptr;
    auto it = block.parsedInputs.find("CLONE_OPTION");
    cloneOptions = &sprite->blocks[it->second.literalValue.asString()];

    Sprite* spriteToClone = getAvailableSprite();
    if(!spriteToClone) return BlockResult::CONTINUE;
    if (cloneOptions->fields["CLONE_OPTION"][0] == "_myself_") {
        *spriteToClone = *sprite;
    } else {
        for (Sprite* currentSprite : sprites) {
            if (currentSprite->name == Math::removeQuotations(cloneOptions->fields["CLONE_OPTION"][0]) && !currentSprite->isClone) {
                *spriteToClone = *currentSprite;
            }
        }
    }
    spriteToClone->blockChains.clear();

    if (spriteToClone != nullptr && !spriteToClone->name.empty()) {
        spriteToClone->isClone = true;
        spriteToClone->isStage = false;
        spriteToClone->toDelete = false;
        spriteToClone->id = Math::generateRandomString(15);
        std::cout << "Created clone of " << sprite->name << std::endl;
        // add clone to sprite list
        sprites.push_back(spriteToClone);
        Sprite* addedSprite = sprites.back();
        // Run "when I start as a clone" scripts for the clone
        for (Sprite* currentSprite : sprites) {
            if (currentSprite == addedSprite) {
                for (auto& [id, block] : currentSprite->blocks) {
                    if (block.opcode == block.CONTROL_START_AS_CLONE) {
                        // std::cout << "Running clone block " << block.id << std::endl;
                        executor.runBlock(block, currentSprite);
                    }
                }
            }
        }
    }
    return BlockResult::CONTINUE;
}
BlockResult ControlBlocks::deleteThisClone(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    if(sprite->isClone)
    sprite->toDelete = true;
    return BlockResult::CONTINUE;
}

BlockResult ControlBlocks::stop(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    std::string stopType = block.fields.at("STOP_OPTION")[0];
    if(stopType == "all"){
        toExit = true;
        return BlockResult::RETURN;
    }
    if(stopType == "this script"){
        for (std::string repeatID : sprite->blockChains[block.blockChainID].blocksToRepeat) {
            Block* repeatBlock = &sprite->blocks[repeatID];
            if (repeatBlock) {
                repeatBlock->repeatTimes = -1;
            }
        }
        sprite->blockChains[block.blockChainID].blocksToRepeat.clear();
        return BlockResult::CONTINUE;
    }

    if(stopType == "other scripts in sprite"){
        for(auto& [id,chain] : sprite->blockChains){
            if(id == block.blockChainID) continue;
            for (std::string repeatID : chain.blocksToRepeat) {
                Block* repeatBlock = &sprite->blocks[repeatID];
                if (repeatBlock) {
                    repeatBlock->repeatTimes = -1;
                }
            }
            chain.blocksToRepeat.clear();
        }
        return BlockResult::CONTINUE;
    }
    return BlockResult::CONTINUE;
}

BlockResult ControlBlocks::startAsClone(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    return BlockResult::CONTINUE;
}

BlockResult ControlBlocks::wait(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    
    if(block.repeatTimes == -1){
        block.repeatTimes = -5;
        
        Value duration = Scratch::getInputValue(block,"DURATION",sprite);
        if(duration.isNumeric()) {
            block.waitDuration = duration.asDouble() * 1000; // convert to milliseconds
        } else {
            block.waitDuration = 0;
        }
        
        block.waitStartTime = std::chrono::high_resolution_clock::now();
        
        BlockExecutor::addToRepeatQueue(sprite, const_cast<Block*>(&block));
    }
    
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - block.waitStartTime).count();
    
    if (elapsedTime >= block.waitDuration) {
        block.repeatTimes = -1;
        sprite->blockChains[block.blockChainID].blocksToRepeat.pop_back();
        return BlockResult::CONTINUE;
    }

    return BlockResult::RETURN;
}

BlockResult ControlBlocks::waitUntil(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    
    if(block.repeatTimes == -1){
        block.repeatTimes = -4;
        BlockExecutor::addToRepeatQueue(sprite, const_cast<Block*>(&block));
    }
    

    Value conditionValue = Scratch::getInputValue(block, "CONDITION", sprite);
    
    bool conditionMet = false;
    if (conditionValue.isNumeric()) {
        conditionMet = conditionValue.asDouble() != 0.0;
    } else {
        conditionMet = !conditionValue.asString().empty();
    }
    
    if (conditionMet) {
        block.repeatTimes = -1;
        sprite->blockChains[block.blockChainID].blocksToRepeat.pop_back();
        return BlockResult::CONTINUE;
    }
    
    return BlockResult::RETURN;
}

BlockResult ControlBlocks::repeat(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){

    if(block.repeatTimes == -1){
        block.repeatTimes = Scratch::getInputValue(block, "TIMES", sprite).asInt();
        BlockExecutor::addToRepeatQueue(sprite,&block);
    }

    if (block.repeatTimes > 0) {
        auto it = block.parsedInputs.find("SUBSTACK");
        if(it != block.parsedInputs.end()){
            Block* subBlock = &sprite->blocks[it->second.blockId];
            if(subBlock){
                executor.runBlock(*subBlock,sprite);
            }
        }

        // Countdown
        block.repeatTimes -= 1;
        return BlockResult::RETURN;
} else {
        block.repeatTimes = -1;
    }
    //std::cout << "done with repeat " << block.id << std::endl;
    sprite->blockChains[block.blockChainID].blocksToRepeat.pop_back();
    return BlockResult::CONTINUE;

}

BlockResult ControlBlocks::repeatUntil(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){

    if(block.repeatTimes == -1){
        block.repeatTimes = -2;
        BlockExecutor::addToRepeatQueue(sprite, &block);
        std::cout << "added to repeat queue!" << std::endl;
    }

    Value conditionValue = Scratch::getInputValue(block,"CONDITION",sprite);
    bool condition = false;
    if(conditionValue.isNumeric()){
        condition = conditionValue.asDouble() != 0.0;
    } else condition = !conditionValue.asString().empty();
    
    if (condition) {
        block.repeatTimes = -1;
        sprite->blockChains[block.blockChainID].blocksToRepeat.pop_back();
        return BlockResult::CONTINUE;
    }
    
        auto it = block.parsedInputs.find("SUBSTACK");
        if(it != block.parsedInputs.end()){
            const std::string& blockId = it->second.blockId;
            auto blockIt = sprite->blocks.find(blockId);
            if (blockIt != sprite->blocks.end()) {
                Block* subBlock = &blockIt->second;
                executor.runBlock(*subBlock, sprite);
            } else {
                std::cerr << "Invalid blockId: " << blockId << std::endl;
            }
        }
    
    // Continue the loop
    return BlockResult::RETURN;
}

BlockResult ControlBlocks::forever(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){

    if(block.repeatTimes == -1){
        block.repeatTimes = -3;
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    auto it = block.parsedInputs.find("SUBSTACK");
    if(it != block.parsedInputs.end()){
        Block* subBlock = &sprite->blocks[it->second.blockId];
        if(subBlock){
            executor.runBlock(*subBlock,sprite);
        }
    }
    return BlockResult::RETURN;
}