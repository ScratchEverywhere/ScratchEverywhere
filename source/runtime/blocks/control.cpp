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
    Value conditionValue;
    if (!Scratch::getInput(block, "CONDITION", thread, sprite, conditionValue)) return BlockResult::REPEAT;

    const bool condition = conditionValue.asBoolean();

    if (condition) {
        Block *substack = block->inputs["SUBSTACK"].block;
        if (substack != nullptr) {
            thread->nextBlock = substack;
            return BlockResult::CONTINUE_IMMEDIATELY;
        }
        return BlockResult::CONTINUE;
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, if_else) {
    Value conditionValue;
    if (!Scratch::getInput(block, "CONDITION", thread, sprite, conditionValue)) return BlockResult::REPEAT;

    const bool condition = conditionValue.asBoolean();
    const std::string key = condition ? "SUBSTACK" : "SUBSTACK2";

    Block *substack = block->inputs[key].block;
    if (substack != nullptr) {
        thread->nextBlock = substack;
        return BlockResult::CONTINUE_IMMEDIATELY;
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, create_clone_of) {
    if (Scratch::cloneCount >= Scratch::maxClones) return BlockResult::CONTINUE;
    Value input;
    if (!Scratch::getInput(block, "CLONE_OPTION", thread, sprite, input)) return BlockResult::REPEAT;

    Sprite *original = nullptr;
    if (input.asString() == "_myself_") {
        original = sprite;
    } else {
        for (Sprite *currentSprite : Scratch::sprites) {
            if (!currentSprite->isClone && !currentSprite->isStage && currentSprite->name == input.asString()) {
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
    spriteToClone->rotationCenterX = original->rotationCenterX;
    spriteToClone->rotationCenterY = original->rotationCenterY;
    spriteToClone->size = original->size;
    spriteToClone->rotation = original->rotation;
    spriteToClone->layer = original->layer;
    spriteToClone->rotationStyle = original->rotationStyle;
    spriteToClone->spriteWidth = original->spriteWidth;
    spriteToClone->spriteHeight = original->spriteHeight;
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
    Scratch::forceRedraw = true;
    Scratch::pendingSprites.push_back(std::make_pair(spriteToClone, original));
    BlockExecutor::runAllBlocksByOpcodeInSprite("control_start_as_clone", spriteToClone);
    Scratch::cloneCount++;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, delete_this_clone) {
    if (!sprite->isClone) return BlockResult::CONTINUE;
    sprite->toDelete = true;
    Scratch::cloneCount--;
    return BlockResult::RETURN;
}

SCRATCH_BLOCK(control, stop) {
    Value stopTypeV;
    if (!Scratch::getInput(block, "STOP_OPTION", thread, sprite, stopTypeV)) return BlockResult::REPEAT;
    std::string stopType = stopTypeV.asString();

    if (stopType == "all") {
        BlockExecutor::stopClicked = true;
        return BlockResult::RETURN;
    };
    if (stopType == "this script") {
        thread->finished = true;
        return BlockResult::RETURN;
    };
    if (stopType == "other scripts in sprite") {
        for (ScriptThread *t : sprite->threads) {
            if (thread == t) continue;
            t->finished = true;
        }
        for (ScriptThread *t : sprite->pendingThreads) {
            if (thread == t) continue;
            t->finished = true;
        }
        for (Sound sound : sprite->sounds)
            SoundPlayer::stopSound(sound.fullName); // does it also stop the sound from the current thread?
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
    Block *substack = block->inputs["SUBSTACK"].block;
    if (substack != nullptr)
        thread->nextBlock = substack;
    else return BlockResult::REPEAT;
    return BlockResult::CONTINUE_IMMEDIATELY;
}

SCRATCH_BLOCK(control, wait_until) {
    Value condition;
    if (!Scratch::getInput(block, "CONDITION", thread, sprite, condition)) return BlockResult::REPEAT;

    if (condition.asBoolean()) return BlockResult::CONTINUE;
    Scratch::resetInput(block);
    return BlockResult::REPEAT;
}

SCRATCH_BLOCK(control, wait) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps == 1) {
        if (state->waitTimer.hasElapsed(state->waitDuration)) {
            thread->eraseState(block);
            return BlockResult::CONTINUE_IMMEDIATELY;
        }
        return BlockResult::REPEAT;
    }
    Value duration;
    if (!Scratch::getInput(block, "DURATION", thread, sprite, duration)) return BlockResult::REPEAT;
    state->waitDuration = duration.asDouble() * 1000;

    state->waitTimer.start();
    Scratch::forceRedraw = true;
    state->completedSteps = 1;
    return BlockResult::REPEAT;
}

SCRATCH_BLOCK(control, repeat) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps == 0) { // start
        Value repeatTimesValue;
        if (!Scratch::getInput(block, "TIMES", thread, sprite, repeatTimesValue)) return BlockResult::REPEAT;
        state->repeatTimes = std::round(repeatTimesValue.asDouble());

        state->completedSteps = 1;
    }
    if (state->repeatTimes <= 0) { // end
        thread->eraseState(block);
        return BlockResult::CONTINUE;
    }

    state->repeatTimes--;

    Block *substack = block->inputs["SUBSTACK"].block;
    if (substack != nullptr)
        thread->nextBlock = substack;
    else return BlockResult::REPEAT;

    return BlockResult::CONTINUE_IMMEDIATELY;
}

SCRATCH_BLOCK(control, while) {
    Value condition;
    if (!Scratch::getInput(block, "CONDITION", thread, sprite, condition)) return BlockResult::REPEAT;

    if (!condition.asBoolean()) return BlockResult::CONTINUE;

    Block *substack = block->inputs["SUBSTACK"].block;
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
    Value condition;
    if (!Scratch::getInput(block, "CONDITION", thread, sprite, condition)) return BlockResult::REPEAT;

    if (condition.asBoolean()) return BlockResult::CONTINUE;

    Block *substack = block->inputs["SUBSTACK"].block;
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
    if (state->completedSteps != 1) {
        Value upperBound;
        if (!Scratch::getInput(block, "VALUE", thread, sprite, upperBound)) return BlockResult::REPEAT;
        state->repeatTimes = 0;
        state->waitDuration = upperBound.asDouble();
        state->completedSteps = 1;
    }

    if (state->repeatTimes >= state->waitDuration) {
        thread->eraseState(block);
        return BlockResult::CONTINUE;
    }
    BlockExecutor::setVariableValue(Scratch::getFieldId(*block, "VARIABLE"), Value(state->repeatTimes + 1), sprite);

    state->repeatTimes++;

    Block *substack = block->inputs["SUBSTACK"].block;
    if (substack != nullptr)
        thread->nextBlock = substack;
    else return BlockResult::REPEAT;

    return BlockResult::CONTINUE_IMMEDIATELY;
}
