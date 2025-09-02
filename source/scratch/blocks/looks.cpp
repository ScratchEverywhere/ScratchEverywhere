#include "looks.hpp"
#include "../unzip.hpp"
#include "blockExecutor.hpp"
#include "image.hpp"
#include "interpret.hpp"
#include "math.hpp"
#include "sprite.hpp"
#include "value.hpp"
#include <algorithm>
#include <cstddef>

BlockResult LooksBlocks::show(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->visible = true;
    if (projectType == UNZIPPED) {
        Image::loadImageFromFile(sprite->costumes[sprite->currentCostume].fullName);
    } else {
        Image::loadImageFromSB3(&Unzip::zipArchive, sprite->costumes[sprite->currentCostume].fullName);
    }
    return BlockResult::CONTINUE;
}
BlockResult LooksBlocks::hide(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->visible = false;
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::switchCostumeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value inputValue = Scratch::getInputValue(block, "COSTUME", sprite);
    std::string inputString = inputValue.asString();

    auto inputFind = block.parsedInputs.find("COSTUME");
    if (inputFind != block.parsedInputs.end() && inputFind->second.inputType == ParsedInput::LITERAL) {
        Block *inputBlock = findBlock(inputValue.asString());
        if (inputBlock != nullptr) {
            if (Scratch::getFieldValue(*inputBlock, "COSTUME") != "")
                inputString = Scratch::getFieldValue(*inputBlock, "COSTUME");
            else return BlockResult::CONTINUE;
        }
    }

    bool imageFound = false;
    for (size_t i = 0; i < sprite->costumes.size(); i++) {
        if (sprite->costumes[i].name == inputString) {
            sprite->currentCostume = i;
            imageFound = true;
            break;
        }
    }
    if (Math::isNumber(inputString) && inputFind != block.parsedInputs.end() && (inputFind->second.inputType == ParsedInput::BLOCK || inputFind->second.inputType == ParsedInput::VARIABLE) && !imageFound) {
        int costumeIndex = inputValue.asInt() - 1;
        if (costumeIndex >= 0 && static_cast<size_t>(costumeIndex) < sprite->costumes.size()) {
            sprite->currentCostume = costumeIndex;
            imageFound = true;
        }
    }

    if (projectType == UNZIPPED) {
        Image::loadImageFromFile(sprite->costumes[sprite->currentCostume].fullName);
    } else {
        Image::loadImageFromSB3(&Unzip::zipArchive, sprite->costumes[sprite->currentCostume].fullName);
    }

    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::nextCostume(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->currentCostume++;
    if (sprite->currentCostume >= static_cast<int>(sprite->costumes.size())) {
        sprite->currentCostume = 0;
    }
    if (projectType == UNZIPPED) {
        Image::loadImageFromFile(sprite->costumes[sprite->currentCostume].fullName);
    } else {
        Image::loadImageFromSB3(&Unzip::zipArchive, sprite->costumes[sprite->currentCostume].fullName);
    }
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::switchBackdropTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value inputValue = Scratch::getInputValue(block, "BACKDROP", sprite);
    std::string inputString = inputValue.asString();

    auto inputFind = block.parsedInputs.find("BACKDROP");
    if (inputFind != block.parsedInputs.end() && inputFind->second.inputType == ParsedInput::LITERAL) {
        Block *inputBlock = findBlock(inputString);
        if (inputBlock != nullptr) {
            if (Scratch::getFieldValue(*inputBlock, "BACKDROP") != "")
                inputString = Scratch::getFieldValue(*inputBlock, "BACKDROP");
            else return BlockResult::CONTINUE;
        }
    }

    for (Sprite *currentSprite : sprites) {
        if (!currentSprite->isStage) {
            continue;
        }

        bool imageFound = false;
        for (size_t i = 0; i < currentSprite->costumes.size(); i++) {
            if (currentSprite->costumes[i].name == inputString) {
                currentSprite->currentCostume = i;
                imageFound = true;
                break;
            }
        }
        if (Math::isNumber(inputString) && inputFind != block.parsedInputs.end() && (inputFind->second.inputType == ParsedInput::BLOCK || inputFind->second.inputType == ParsedInput::VARIABLE) && !imageFound) {
            int costumeIndex = inputValue.asInt() - 1;
            if (costumeIndex >= 0 && static_cast<size_t>(costumeIndex) < currentSprite->costumes.size()) {
                imageFound = true;
                currentSprite->currentCostume = costumeIndex;
            }
        }

        if (projectType == UNZIPPED) {
            Image::loadImageFromFile(currentSprite->costumes[currentSprite->currentCostume].fullName);
        } else {
            Image::loadImageFromSB3(&Unzip::zipArchive, currentSprite->costumes[currentSprite->currentCostume].fullName);
        }
    }

    for (auto &currentSprite : sprites) {
        for (auto &[id, spriteBlock] : currentSprite->blocks) {
            if (spriteBlock.opcode != "event_whenbackdropswitchesto") continue;
            try {
                if (Scratch::getFieldValue(spriteBlock, "BACKDROP") == sprite->costumes[sprite->currentCostume].name) {
                    executor.runBlock(spriteBlock, currentSprite, withoutScreenRefresh, fromRepeat);
                }
            } catch (...) {
                continue;
            }
        }
    }

    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::nextBackdrop(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    for (Sprite *currentSprite : sprites) {
        if (!currentSprite->isStage) {
            continue;
        }
        currentSprite->currentCostume++;
        if (currentSprite->currentCostume >= static_cast<int>(currentSprite->costumes.size())) {
            currentSprite->currentCostume = 0;
        }
        if (projectType == UNZIPPED) {
            Image::loadImageFromFile(currentSprite->costumes[currentSprite->currentCostume].fullName);
        } else {
            Image::loadImageFromSB3(&Unzip::zipArchive, currentSprite->costumes[currentSprite->currentCostume].fullName);
        }
    }

    for (auto &currentSprite : sprites) {
        for (auto &[id, spriteBlock] : currentSprite->blocks) {
            if (spriteBlock.opcode != "event_whenbackdropswitchesto") continue;
            try {
                if (Scratch::getFieldValue(spriteBlock, "BACKDROP") == sprite->costumes[sprite->currentCostume].name) {
                    executor.runBlock(spriteBlock, currentSprite, withoutScreenRefresh, fromRepeat);
                }
            } catch (...) {
                continue;
            }
        }
    }

    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::goForwardBackwardLayers(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value value = Scratch::getInputValue(block, "NUM", sprite);
    std::string forwardBackward = Scratch::getFieldValue(block, "FORWARD_BACKWARD");
    ;
    if (!value.isNumeric()) return BlockResult::CONTINUE;

    int shift = value.asInt();
    if (shift == 0) return BlockResult::CONTINUE;

    if (forwardBackward == "forward") {
        int targetLayer = sprite->layer + shift;

        for (Sprite *currentSprite : sprites) {
            if (currentSprite->isStage || currentSprite == sprite) continue;
            if (currentSprite->layer >= targetLayer) {
                currentSprite->layer++;
            }
        }

        sprite->layer = targetLayer;

    } else if (forwardBackward == "backward") {
        int targetLayer = sprite->layer - shift;

        for (Sprite *currentSprite : sprites) {
            if (currentSprite->isStage || currentSprite == sprite) continue;
            if (currentSprite->layer <= targetLayer) {
                currentSprite->layer--;
                if (currentSprite->layer < 0) currentSprite->layer = 0;
            }
        }

        sprite->layer = targetLayer;
    }

    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::goToFrontBack(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::string value = Scratch::getFieldValue(block, "FRONT_BACK");
    ;
    if (value == "front") {

        double maxLayer = 0.0;
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->layer > maxLayer) {
                maxLayer = currentSprite->layer;
            }
        }

        sprite->layer = maxLayer + 1;

    } else if (value == "back") {
        double minLayer = std::numeric_limits<double>::max();
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->isStage) continue;
            if (currentSprite->layer < minLayer) {
                minLayer = currentSprite->layer;
            }
        }

        sprite->layer = minLayer - 1;
    }
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::setSizeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value value = Scratch::getInputValue(block, "SIZE", sprite);

    // hasn't been rendered yet, or fencing is disabled
    if ((sprite->spriteWidth < 1 || sprite->spriteHeight < 1) || !Scratch::fencing) {
        sprite->size = value.asDouble();
        return BlockResult::CONTINUE;
    }

    if (value.isNumeric()) {
        const double inputSizePercent = value.asDouble();

        const double minScale = std::min(1.0, std::max(5.0 / sprite->spriteWidth, 5.0 / sprite->spriteHeight));

        const double maxScale = std::min((1.5 * Scratch::projectWidth) / sprite->spriteWidth, (1.5 * Scratch::projectHeight) / sprite->spriteHeight);

        const double clampedScale = std::clamp(inputSizePercent / 100.0, minScale, maxScale);
        sprite->size = clampedScale * 100.0;
    }
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::changeSizeBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value value = Scratch::getInputValue(block, "CHANGE", sprite);

    // hasn't been rendered yet, or fencing is disabled
    if ((sprite->spriteWidth < 1 || sprite->spriteHeight < 1) || !Scratch::fencing) {
        sprite->size += value.asDouble();
        return BlockResult::CONTINUE;
    }

    if (value.isNumeric()) {
        sprite->size += value.asDouble();

        double minScale = std::min(1.0, std::max(5.0 / sprite->spriteWidth, 5.0 / sprite->spriteHeight)) * 100.0;

        double maxScale = std::min((1.5 * Scratch::projectWidth) / sprite->spriteWidth, (1.5 * Scratch::projectHeight) / sprite->spriteHeight) * 100.0;

        sprite->size = std::clamp(static_cast<double>(sprite->size), minScale, maxScale);
    }
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::setEffectTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {

    std::string effect = Scratch::getFieldValue(block, "EFFECT");
    ;
    Value amount = Scratch::getInputValue(block, "VALUE", sprite);

    if (!amount.isNumeric()) return BlockResult::CONTINUE;

    if (effect == "COLOR") {
        // doable....
    } else if (effect == "FISHEYE") {
        // blehhh
    } else if (effect == "WHIRL") {
        // blehhh
    } else if (effect == "PIXELATE") {
        // blehhh
    } else if (effect == "MOSAIC") {
        // blehhh
    } else if (effect == "BRIGHTNESS") {
        // doable....
    } else if (effect == "GHOST") {
        sprite->ghostEffect = std::clamp(amount.asDouble(), 0.0, 100.0);
    } else {
    }

    return BlockResult::CONTINUE;
}
BlockResult LooksBlocks::changeEffectBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::string effect = Scratch::getFieldValue(block, "EFFECT");
    ;
    Value amount = Scratch::getInputValue(block, "CHANGE", sprite);

    if (!amount.isNumeric()) return BlockResult::CONTINUE;

    if (effect == "COLOR") {
        // doable....
    } else if (effect == "FISHEYE") {
        // blehhh
    } else if (effect == "WHIRL") {
        // blehhh
    } else if (effect == "PIXELATE") {
        // blehhh
    } else if (effect == "MOSAIC") {
        // blehhh
    } else if (effect == "BRIGHTNESS") {
        // doable....
    } else if (effect == "GHOST") {
        sprite->ghostEffect += amount.asDouble();
        sprite->ghostEffect = std::clamp(sprite->ghostEffect, 0.0f, 100.0f);
    } else {
    }
    return BlockResult::CONTINUE;
}
BlockResult LooksBlocks::clearGraphicEffects(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {

    sprite->ghostEffect = 0.0f;
    sprite->colorEffect = -99999;

    return BlockResult::CONTINUE;
}

Value LooksBlocks::size(Block &block, Sprite *sprite) {
    return Value(sprite->size);
}

Value LooksBlocks::costume(Block &block, Sprite *sprite) {
    return Value(Scratch::getFieldValue(block, "COSTUME"));
}

Value LooksBlocks::backdrops(Block &block, Sprite *sprite) {
    return Value(Scratch::getFieldValue(block, "BACKDROP"));
}

Value LooksBlocks::costumeNumberName(Block &block, Sprite *sprite) {
    std::string value = Scratch::getFieldValue(block, "NUMBER_NAME");
    ;
    if (value == "name") {
        return Value(sprite->costumes[sprite->currentCostume].name);
    } else if (value == "number") {
        return Value(sprite->currentCostume + 1);
    }
    return Value();
}

Value LooksBlocks::backdropNumberName(Block &block, Sprite *sprite) {
    std::string value = Scratch::getFieldValue(block, "NUMBER_NAME");
    ;
    if (value == "name") {
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->isStage) {
                return Value(currentSprite->costumes[currentSprite->currentCostume].name);
            }
        }
    } else if (value == "number") {
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->isStage) {
                return Value(currentSprite->currentCostume + 1);
            }
        }
    }
    return Value();
}
