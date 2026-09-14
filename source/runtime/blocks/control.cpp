#include "audiostack.hpp"
#include "blockUtils.hpp"
#include "runtime.hpp"
#include "runtime/blockExecutor.hpp"
#include <audio.hpp>
#include <blockExecutor.hpp>
#include <iostream>
#include <math.hpp>
#include <os.hpp>
#include <ostream>
#include <sprite.hpp>
#include <value.hpp>

SCRATCH_BLOCK(control, if) {
    bool condition;
    if (!Scratch::getInputValueAs(block, "CONDITION", thread, sprite, condition)) return BlockResult::REPEAT;

    if (condition) {
        const ParsedInput *input = Scratch::getInput(block, "SUBSTACK");
        if (input == nullptr) return BlockResult::CONTINUE;

        Block *substack = input->block;
        if (substack != nullptr) {
            thread->nextBlock = substack;
            return BlockResult::CONTINUE_IMMEDIATELY;
        }
        return BlockResult::CONTINUE;
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, if_else) {
    bool condition;
    if (!Scratch::getInputValueAs(block, "CONDITION", thread, sprite, condition)) return BlockResult::REPEAT;

    const std::string key = condition ? "SUBSTACK" : "SUBSTACK2";

    const ParsedInput *input = Scratch::getInput(block, key);
    if (input == nullptr) return BlockResult::CONTINUE;

    Block *substack = input->block;
    if (substack != nullptr) {
        thread->nextBlock = substack;
        return BlockResult::CONTINUE_IMMEDIATELY;
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, create_clone_of) {
    if (Scratch::cloneCount >= Scratch::maxClones) return BlockResult::CONTINUE;
    std::string input;
    if (!Scratch::getInputValueAs(block, "CLONE_OPTION", thread, sprite, input)) return BlockResult::REPEAT;

    Sprite *original = nullptr;
    if (input == "_myself_") {
        original = sprite;
    } else {
        for (Sprite *currentSprite : Scratch::sprites) {
            if (!currentSprite->isClone && !currentSprite->isStage && currentSprite->name == input) {
                original = currentSprite;
                break;
            }
        }
    }

    if (!original) return BlockResult::CONTINUE;
    Sprite *spriteToClone = new Sprite();
    spriteToClone->name = original->name;
    spriteToClone->isStage = false;
    spriteToClone->draggable = original->draggable;
    spriteToClone->visible = original->visible;
    spriteToClone->isClone = true;
    spriteToClone->toDelete = false;
    spriteToClone->shouldDoSpriteClick = original->shouldDoSpriteClick;
    spriteToClone->currentCostume = original->currentCostume;
    spriteToClone->xPosition = original->xPosition;
    spriteToClone->yPosition = original->yPosition;
    spriteToClone->size = original->size;
    spriteToClone->rotation = original->rotation;
    spriteToClone->layer = original->layer;
    spriteToClone->rotationStyle = original->rotationStyle;
    spriteToClone->spriteWidth = original->spriteWidth;
    spriteToClone->spriteHeight = original->spriteHeight;
    spriteToClone->instrument = original->instrument;
    spriteToClone->ghostEffect = original->ghostEffect;
    spriteToClone->brightnessEffect = original->brightnessEffect;
    spriteToClone->colorEffect = original->colorEffect;
    spriteToClone->volume = original->volume;
    spriteToClone->pitch = original->pitch;
    spriteToClone->pan = original->pan;
    spriteToClone->penData = original->penData;
    spriteToClone->textToSpeechData = original->textToSpeechData;
    spriteToClone->variables = original->variables;
    spriteToClone->lists = original->lists;
    spriteToClone->sounds = original->sounds;
    spriteToClone->costumes = original->costumes;
    spriteToClone->broadcasts = original->broadcasts;
    spriteToClone->renderInfo.forceUpdate = true;
    spriteToClone->hats = original->hats;
#ifdef ENABLE_CACHING
    BlockExecutor::linkPointers(spriteToClone);
#endif

    int sourceIndex = (Scratch::sprites.size() - 1) - original->layer;
    if (sourceIndex < 0) sourceIndex = 0;
    if (sourceIndex >= (int)Scratch::sprites.size()) sourceIndex = Scratch::sprites.size() - 1;

    auto it = Scratch::sprites.insert(Scratch::sprites.begin() + sourceIndex + 1, spriteToClone);

    for (size_t i = 0; i < sourceIndex + 1; i++) {
        Scratch::sprites[i]->layer = (Scratch::sprites.size() - 1) - i;
    }
    BlockExecutor::sortSprites = true;

    BlockExecutor::runAllBlocksByOpcodeInSprite("control_start_as_clone", spriteToClone);
    Scratch::cloneCount++;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, delete_this_clone) {
    if (!sprite->isClone) return BlockResult::CONTINUE;
    sprite->toDelete = true;
    for (ScriptThread *t : BlockExecutor::threads) {
        if (t->sprite == sprite) t->finished = true;
    }
    Scratch::cloneCount--;
    return BlockResult::RETURN;
}

SCRATCH_BLOCK(control, stop) {
    std::string stopType;
    if (!Scratch::getInputValueAs(block, "STOP_OPTION", thread, sprite, stopType)) return BlockResult::REPEAT;

    if (stopType == "all") {
        BlockExecutor::stopClicked = true;
        for (Sprite *currentSprite : Scratch::sprites) {
            for (ScriptThread *t : BlockExecutor::threads) {
                if (t->sprite->isClone) t->finished = true;
            }
        }
        return BlockResult::RETURN;
    };
    if (stopType == "this script") {
        thread->finished = true;
        return BlockResult::RETURN;
    };
    if (stopType == "other scripts in sprite") {
        for (ScriptThread *t : BlockExecutor::threads) {
            if (thread == t || thread->parentThread == t || t->sprite != sprite) continue;
            t->finished = true;
        }
        for (Sound sound : sprite->sounds)
            Mixer::stopSound(sound.fullName);
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, start_as_clone) {
    if (!sprite->isClone) return BlockResult::CONTINUE;
    thread->finished = false;
    return BlockResult::CONTINUE;
}

SCRATCH_SHADOW_BLOCK(control_create_clone_of_menu, CLONE_OPTION)

SCRATCH_BLOCK(control, forever) {
    const ParsedInput *input = Scratch::getInput(block, "SUBSTACK");
    if (input == nullptr) return BlockResult::REPEAT;

    Block *substack = input->block;
    if (substack != nullptr)
        thread->nextBlock = substack;
    else return BlockResult::REPEAT;
    return BlockResult::CONTINUE_IMMEDIATELY;
}

SCRATCH_BLOCK(control, wait_until) {
    bool condition;
    if (!Scratch::getInputValueAs(block, "CONDITION", thread, sprite, condition)) return BlockResult::REPEAT;

    if (condition) return BlockResult::CONTINUE;
    Scratch::resetInput(block);
    return BlockResult::REPEAT;
}

SCRATCH_BLOCK(control, wait) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps == 1) {
        if (state->waitTimer.hasElapsed(state->waitDuration)) {
            thread->eraseState(block);
            return BlockResult::CONTINUE;
        }
        return BlockResult::REPEAT;
    }

    double duration;
    if (!Scratch::getInputValueAs(block, "DURATION", thread, sprite, duration)) return BlockResult::REPEAT;
    state->waitDuration = duration * 1000;

    state->waitTimer.start();
    Scratch::forceRedraw = true;
    state->completedSteps = 1;
    return BlockResult::REPEAT;
}

SCRATCH_BLOCK(control, repeat) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps == 0) { // start
        double repeatTimes;
        if (!Scratch::getInputValueAs(block, "TIMES", thread, sprite, repeatTimes)) return BlockResult::REPEAT;
        state->repeatTimes = std::round(repeatTimes);

        state->completedSteps = 1;
    }
    if (state->repeatTimes <= 0) { // end
        thread->eraseState(block);
        return BlockResult::CONTINUE;
    }

    state->repeatTimes--;

    const ParsedInput *input = Scratch::getInput(block, "SUBSTACK");
    if (input == nullptr) return BlockResult::REPEAT;

    Block *substack = input->block;
    if (substack != nullptr)
        thread->nextBlock = substack;
    else return BlockResult::REPEAT;

    return BlockResult::CONTINUE_IMMEDIATELY;
}

SCRATCH_BLOCK(control, while) {
    bool condition;
    if (!Scratch::getInputValueAs(block, "CONDITION", thread, sprite, condition)) return BlockResult::REPEAT;

    if (!condition) return BlockResult::CONTINUE;

    const ParsedInput *input = Scratch::getInput(block, "SUBSTACK");
    if (input == nullptr) {
        Scratch::resetInput(block);
        return BlockResult::REPEAT;
    }

    Block *substack = input->block;
    if (substack != nullptr)
        thread->nextBlock = substack;
    else {
        Scratch::resetInput(block);
        return BlockResult::REPEAT;
    }
    Scratch::resetInput(block);
    return BlockResult::CONTINUE_IMMEDIATELY;
}

SCRATCH_BLOCK(control, repeat_until) {
    bool condition;
    if (!Scratch::getInputValueAs(block, "CONDITION", thread, sprite, condition)) return BlockResult::REPEAT;

    if (condition) return BlockResult::CONTINUE;

    const ParsedInput *input = Scratch::getInput(block, "SUBSTACK");
    if (input == nullptr) {
        Scratch::resetInput(block);
        return BlockResult::REPEAT;
    }

    Block *substack = input->block;
    if (substack != nullptr)
        thread->nextBlock = substack;
    else {
        Scratch::resetInput(block);
        return BlockResult::REPEAT;
    }
    Scratch::resetInput(block);
    return BlockResult::CONTINUE_IMMEDIATELY;
}

SCRATCH_BLOCK(control, get_counter) {
    *outValue = Value(Scratch::counter);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, incr_counter) {
    Scratch::counter++;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, clear_counter) {
    Scratch::counter = 0;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, for_each) {
    BlockState *state = thread->getState(block);

    double upperBound;
    if (!Scratch::getInputValueAs(block, "VALUE", thread, sprite, upperBound)) return BlockResult::REPEAT;

    if (state->completedSteps != 1) {
        state->repeatTimes = 0;
        state->completedSteps = 1;
    }

    if (state->repeatTimes >= upperBound) {
        thread->eraseState(block);
        return BlockResult::CONTINUE;
    }

    BlockExecutor::setVariableValue(Scratch::getFieldId(*block, "VARIABLE"), Value(state->repeatTimes + 1), sprite);

    state->repeatTimes++;

    const ParsedInput *input = Scratch::getInput(block, "SUBSTACK");
    if (input == nullptr) return BlockResult::REPEAT;

    Block *substack = input->block;
    if (substack != nullptr)
        thread->nextBlock = substack;
    else return BlockResult::REPEAT;

    return BlockResult::CONTINUE_IMMEDIATELY;
}
