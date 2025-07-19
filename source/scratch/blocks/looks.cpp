#include "looks.hpp"


BlockResult LooksBlocks::show(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh) {
    sprite->visible = true;
    return BlockResult::CONTINUE;
}
BlockResult LooksBlocks::hide(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh) {
    sprite->visible = false;
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::switchCostumeTo(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    Value inputValue = Scratch::getInputValue(block,"COSTUME",sprite);
    std::string inputString = inputValue.asString();

    auto inputFind = block.parsedInputs.find("COSTUME");
    if(inputFind != block.parsedInputs.end() && inputFind->second.inputType == ParsedInput::LITERAL){
        Block* inputBlock = findBlock(inputValue.asString());
        if(inputBlock != nullptr){
            inputString = inputBlock->fields["COSTUME"][0].get<std::string>();
        }
    }


    bool foundImage = false;
        for (size_t i = 0; i < sprite->costumes.size(); i++) {
            if (sprite->costumes[i].name == inputString) {
                if((size_t)sprite->currentCostume != i){
                   // Image::queueFreeImage(sprite->costumes[sprite->currentCostume].id);
                }
                sprite->currentCostume = i;
                foundImage = true;
                break;
            }
        }
        if(!foundImage && inputValue.isNumeric()){
            int costumeIndex = inputValue.asInt() - 1;
            if (costumeIndex >= 0 && static_cast<size_t>(costumeIndex) < sprite->costumes.size()) {
                if(sprite->currentCostume != costumeIndex){
                   // Image::queueFreeImage(sprite->costumes[sprite->currentCostume].id);
            }
            foundImage = true;
            sprite->currentCostume = costumeIndex;
        }
        }

        if(projectType == UNZIPPED){
            Image::loadImageFromFile(sprite->costumes[sprite->currentCostume].id);
        }

    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::nextCostume(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh) {
    //Image::queueFreeImage(sprite->costumes[sprite->currentCostume].id);
    sprite->currentCostume++;
    if (sprite->currentCostume >= static_cast<int>(sprite->costumes.size())) {
        sprite->currentCostume = 0;
    }
    if(projectType == UNZIPPED){
        Image::loadImageFromFile(sprite->costumes[sprite->currentCostume].id);
    }
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::switchBackdropTo(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    Value inputValue = Scratch::getInputValue(block, "BACKDROP", sprite);
    std::string inputString = inputValue.asString();
    
    auto inputFind = block.parsedInputs.find("BACKDROP");
    if(inputFind != block.parsedInputs.end() && inputFind->second.inputType == ParsedInput::LITERAL){
        Block* inputBlock = findBlock(inputValue.asString());
        if(inputBlock != nullptr){
            inputString = inputBlock->fields["BACKDROP"][0].get<std::string>();
        }
    }
    
    for(Sprite* currentSprite : sprites){
        if(!currentSprite->isStage){
            continue;
        }
        
        bool foundImage = false;
        
        for (size_t i = 0; i < currentSprite->costumes.size(); i++) {
            if (currentSprite->costumes[i].name == inputString) {
                if((size_t)currentSprite->currentCostume != i){
                   // Image::queueFreeImage(currentSprite->costumes[currentSprite->currentCostume].id);
                }
                currentSprite->currentCostume = i;
                foundImage = true;
                break;
            }
        }
        if(!foundImage && inputValue.isNumeric()){
            int costumeIndex = inputValue.asInt() - 1;
            if (costumeIndex >= 0 && static_cast<size_t>(costumeIndex) < currentSprite->costumes.size()) {
                if(currentSprite->currentCostume != costumeIndex){
                    //Image::queueFreeImage(currentSprite->costumes[currentSprite->currentCostume].id);
                }
                foundImage = true;
                currentSprite->currentCostume = costumeIndex;
            }
        }
        
        if(projectType == UNZIPPED){
            Image::loadImageFromFile(currentSprite->costumes[currentSprite->currentCostume].id);
        }
    }
    
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::nextBackdrop(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh) {
    for(Sprite* currentSprite : sprites){
        if(!currentSprite->isStage){
            continue;
        }
        //Image::queueFreeImage(currentSprite->costumes[currentSprite->currentCostume].id);
        currentSprite->currentCostume++;
        if (currentSprite->currentCostume >= static_cast<int>(currentSprite->costumes.size())) {
            currentSprite->currentCostume = 0;
        }
        if(projectType == UNZIPPED){
            Image::loadImageFromFile(currentSprite->costumes[currentSprite->currentCostume].id);
        }
}
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::goForwardBackwardLayers(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    Value value = Scratch::getInputValue(block,"NUM",sprite);
    std::string forwardBackward = block.fields.at("FORWARD_BACKWARD")[0];
    if (value.isNumeric()) {
    if (forwardBackward == "forward") {

        // check if a sprite is already on the same layer
        for(Sprite* currentSprite : sprites){
            if(currentSprite->isStage) continue;
            if(currentSprite->layer == (currentSprite->layer + value.asInt())){
                for(Sprite* moveupSprite : sprites){
                    if(moveupSprite->isStage || !(moveupSprite->layer >= (currentSprite->layer + value.asInt()))) continue;
                    moveupSprite-> layer++;
                }
            }
        }

        sprite->layer += value.asInt();

    } else if (forwardBackward == "backward") {

        // check if a sprite is already on the same layer
        for(Sprite* currentSprite : sprites){
            if(currentSprite->isStage) continue;
            if(currentSprite->layer == (currentSprite->layer - value.asInt())){
                for(Sprite* moveupSprite : sprites){
                    if(moveupSprite->isStage || !(moveupSprite->layer >= (currentSprite->layer - value.asInt()))) continue;
                    moveupSprite-> layer++;
                }
            }
        }

        sprite->layer -= value.asInt();
        if(sprite->layer < 1){
            for(Sprite* currentSprite : sprites){
            if(currentSprite->isStage) continue;
            currentSprite->layer+= 2;
        }
        sprite->layer = 0;
        }
    }
}
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::goToFrontBack(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    std::string value = block.fields.at("FRONT_BACK")[0];
    if (value == "front") {

        double maxLayer = 0.0;
        for (Sprite* currentSprite : sprites) {
            if (currentSprite->layer > maxLayer) {
                maxLayer = currentSprite->layer;
            }
        }

        sprite->layer = maxLayer + 1;

    } else if (value == "back") {
        for(Sprite* currentSprite : sprites){
            if(currentSprite->isStage) continue;
            currentSprite->layer+= 2;
        }
        sprite->layer = 0;
    }
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::setSizeTo(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh) {
    Value value = Scratch::getInputValue(block, "SIZE", sprite);
    if (value.isNumeric()) {
        const double inputSizePercent = value.asDouble();

        const double minScale = std::min(1.0, std::max(5.0 / sprite->spriteWidth, 5.0 / sprite->spriteHeight));

        const double maxScale = std::min((1.5 * Scratch::projectWidth) / sprite->spriteWidth, (1.5 * Scratch::projectHeight) / sprite->spriteHeight);

        const double clampedScale = std::clamp(inputSizePercent / 100.0, minScale, maxScale);
        sprite->size = clampedScale * 100.0;
    }
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::changeSizeBy(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh) {
    Value value = Scratch::getInputValue(block, "CHANGE", sprite);
    if (value.isNumeric()) {
        sprite->size += value.asDouble();

        double minScale = std::min(1.0, std::max(5.0 / sprite->spriteWidth, 5.0 / sprite->spriteHeight)) * 100.0;

        double maxScale = std::min((1.5 * Scratch::projectWidth) / sprite->spriteWidth, (1.5 * Scratch::projectHeight) / sprite->spriteHeight) * 100.0;

        sprite->size = std::clamp(static_cast<double>(sprite->size), minScale, maxScale);
    }
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::setEffectTo(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    
    std::string effect = block.fields.at("EFFECT")[0];
    Value amount = Scratch::getInputValue(block,"VALUE",sprite);

    if(!amount.isNumeric()) return BlockResult::CONTINUE;

    if (effect == "COLOR") {
        // doable....
    } else if (effect == "FISHEYE") {
        // blehhh
    } else if (effect == "WHIRL") {
        // blehhh
    }else if (effect == "PIXELATE") {
        // blehhh
    }else if (effect == "MOSAIC") {
        // blehhh
    }else if (effect == "BRIGHTNESS") {
        // doable....
    }else if (effect == "GHOST") {
        sprite->ghostEffect = std::clamp(amount.asInt(),0,100);
    }
    else {
       std::cerr << "what effect did you even put??" << std::endl;
    }
    
return BlockResult::CONTINUE;

}
BlockResult LooksBlocks::changeEffectBy(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){
    std::string effect = block.fields.at("EFFECT")[0];
    Value amount = Scratch::getInputValue(block,"CHANGE",sprite);

    if(!amount.isNumeric()) return BlockResult::CONTINUE;

    if (effect == "COLOR") {
        // doable....
    } else if (effect == "FISHEYE") {
        // blehhh
    } else if (effect == "WHIRL") {
        // blehhh
    }else if (effect == "PIXELATE") {
        // blehhh
    }else if (effect == "MOSAIC") {
        // blehhh
    }else if (effect == "BRIGHTNESS") {
        // doable....
    }else if (effect == "GHOST") {
        sprite->ghostEffect += amount.asInt();
        sprite->ghostEffect = std::clamp(sprite->ghostEffect,0,100);
    }
    else {
       std::cerr << "what effect did you even put??" << std::endl;
    }
return BlockResult::CONTINUE;
}
BlockResult LooksBlocks::clearGraphicEffects(Block& block, Sprite* sprite, Block** waitingBlock, bool* withoutScreenRefresh){

sprite->ghostEffect = 0;
sprite->colorEffect = -99999;

return BlockResult::CONTINUE;
}

Value LooksBlocks::size(Block& block, Sprite* sprite) {
    return Value(sprite->size);
}

Value LooksBlocks::costume(Block& block, Sprite* sprite) {
    return Value(block.fields.at("COSTUME")[0].get<std::string>());
}

Value LooksBlocks::backdrops(Block& block, Sprite* sprite) {
    return Value(block.fields.at("BACKDROP")[0].get<std::string>());
}

Value LooksBlocks::costumeNumberName(Block& block, Sprite* sprite) {
    std::string value = block.fields.at("NUMBER_NAME")[0];
    if (value == "name") {
        std::cout << sprite->costumes[sprite->currentCostume].name << std::endl;
        return Value(sprite->costumes[sprite->currentCostume].name);
    } else if (value == "number") {
        return Value(sprite->currentCostume + 1);
    }
    return Value();
}

Value LooksBlocks::backdropNumberName(Block& block, Sprite* sprite) {
    std::string value = block.fields.at("NUMBER_NAME")[0];
    if (value == "name") {
        for (Sprite* currentSprite : sprites) {
            if (currentSprite->isStage) {
                return Value(currentSprite->costumes[currentSprite->currentCostume].name);
            }
        }
    } else if (value == "number") {
        for (Sprite* currentSprite : sprites) {
            if (currentSprite->isStage) {
                return Value(currentSprite->currentCostume + 1);
            }
        }
    }
    return Value();
}