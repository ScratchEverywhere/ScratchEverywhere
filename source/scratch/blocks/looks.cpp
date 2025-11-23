#include "blockUtils.hpp"
#include "image.hpp"
#include "interpret.hpp"
#include "sprite.hpp"
#include "value.hpp"
#include <algorithm>
#include <cstddef>

SCRATCH_BLOCK(looks, show) {
    sprite->visible = true;
    Image::loadImageFromProject(sprite);
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, hide) {
    sprite->visible = false;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, switchcostumeto) {
    Value inputValue = Scratch::getInputValue(block, "COSTUME", sprite);

    if (inputValue.isDouble()) {
        Scratch::switchCostume(sprite, inputValue.isNaN() ? 0 : inputValue.asDouble() - 1);
        return BlockResult::CONTINUE;
    }

    for (size_t i = 0; i < sprite->costumes.size(); i++) {
        if (sprite->costumes[i].name == inputValue.asString()) {
            Scratch::switchCostume(sprite, i);
            return BlockResult::CONTINUE;
        }
    }

    if (inputValue.asString() == "next costume") {
        Scratch::switchCostume(sprite, ++sprite->currentCostume);
        return BlockResult::CONTINUE;
    } else if (inputValue.asString() == "previous costume") {
        Scratch::switchCostume(sprite, --sprite->currentCostume);
        return BlockResult::CONTINUE;
    }

    if (inputValue.isNumeric()) {
        Scratch::switchCostume(sprite, inputValue.asDouble() - 1);
        return BlockResult::CONTINUE;
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, nextcostume) {
    Scratch::switchCostume(sprite, ++sprite->currentCostume);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, switchbackdropto) {
    Value inputValue = Scratch::getInputValue(block, "BACKDROP", sprite);

    for (Sprite *stage : sprites) {
        if (!stage->isStage) {
            continue;
        }

        if (inputValue.isDouble()) {
            Scratch::switchCostume(stage, inputValue.isNaN() ? 0 : inputValue.asDouble() - 1);
            return BlockResult::CONTINUE;
        }

        for (size_t i = 0; i < stage->costumes.size(); i++) {
            if (stage->costumes[i].name == inputValue.asString()) {
                Scratch::switchCostume(stage, i);
                return BlockResult::CONTINUE;
            }
        }

        if (inputValue.asString() == "next backdrop") {
            Scratch::switchCostume(stage, ++stage->currentCostume);
            return BlockResult::CONTINUE;
        } else if (inputValue.asString() == "previous backdrop") {
            Scratch::switchCostume(stage, --stage->currentCostume);
            return BlockResult::CONTINUE;
        } else if (inputValue.asString() == "random backdrop") {
            if (stage->costumes.size() == 1) break;
            int randomIndex = std::rand() % (stage->costumes.size() - 1);
            if (randomIndex >= stage->currentCostume) randomIndex++;
            Scratch::switchCostume(stage, randomIndex);
            return BlockResult::CONTINUE;
        }

        if (inputValue.isNumeric()) {
            Scratch::switchCostume(stage, inputValue.asDouble() - 1);
            return BlockResult::CONTINUE;
        }

        for (auto &currentSprite : sprites) {
            for (auto &[id, spriteBlock] : currentSprite->blocks) {
                if (spriteBlock.opcode == "event_whenbackdropswitchesto") {
                    if (Scratch::getFieldValue(spriteBlock, "BACKDROP") == stage->costumes[stage->currentCostume].name) {
                        executor.runBlock(spriteBlock, currentSprite, withoutScreenRefresh, fromRepeat);
                    }
                }
            }
        }
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, nextbackdrop) {
    for (Sprite *stage : sprites) {
        if (!stage->isStage) {
            continue;
        }
        Scratch::switchCostume(stage, stage->currentCostume++);

        for (auto &currentSprite : sprites) {
            for (auto &[id, spriteBlock] : currentSprite->blocks) {
                if (spriteBlock.opcode == "event_whenbackdropswitchesto") {
                    if (Scratch::getFieldValue(spriteBlock, "BACKDROP") == stage->costumes[stage->currentCostume].name) {
                        executor.runBlock(spriteBlock, currentSprite, withoutScreenRefresh, fromRepeat);
                    }
                }
            }
        }
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, goforwardbackwardlayers) {
    Value value = Scratch::getInputValue(block, "NUM", sprite);
    std::string forwardBackward = Scratch::getFieldValue(block, "FORWARD_BACKWARD");
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

    Scratch::forceRedraw = true;
    Scratch::sortSprites();
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, gotofrontback) {
    std::string value = Scratch::getFieldValue(block, "FRONT_BACK");
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
    Scratch::forceRedraw = true;
    Scratch::sortSprites();
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, setsizeto) {
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
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, changesizeby) {
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
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, seteffectto) {

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
        sprite->brightnessEffect = std::clamp(amount.asDouble(), -100.0, 100.0);
    } else if (effect == "GHOST") {
        sprite->ghostEffect = std::clamp(amount.asDouble(), 0.0, 100.0);
    } else {
    }

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, changeeffectby) {
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
        sprite->brightnessEffect += amount.asDouble();
        sprite->brightnessEffect = std::clamp(sprite->brightnessEffect, -100.0f, 100.0f);
    } else if (effect == "GHOST") {
        sprite->ghostEffect += amount.asDouble();
        sprite->ghostEffect = std::clamp(sprite->ghostEffect, 0.0f, 100.0f);
    } else {
    }
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, cleargraphiceffects) {

    sprite->ghostEffect = 0.0f;
    sprite->colorEffect = -99999;
    sprite->brightnessEffect = 0.0f;

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_REPORTER_BLOCK(looks, size) {
    return Value(sprite->size);
}

SCRATCH_REPORTER_BLOCK(looks, costumenumbername) {
    std::string value = Scratch::getFieldValue(block, "NUMBER_NAME");
    ;
    if (value == "name") {
        return Value(sprite->costumes[sprite->currentCostume].name);
    } else if (value == "number") {
        return Value(sprite->currentCostume + 1);
    }
    return Value();
}

SCRATCH_REPORTER_BLOCK(looks, backdropnumbername) {
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
