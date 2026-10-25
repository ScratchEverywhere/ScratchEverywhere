#include "audiostack.hpp"
#include "blockUtils.hpp"
#include "runtime.hpp"
#include "runtime/blockExecutor.hpp"
#include <audio.hpp>
#include <blockExecutor.hpp>
#include <bytecode_executor.hpp>
#include <iostream>
#include <math.hpp>
#include <os.hpp>
#include <ostream>
#include <types.hpp>
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
SCRATCH_BYTECODE_BLOCK(control, if) {
    outFirstInstr = compiler.here();
    BytecodeIndex exitJump = compiler.compileConditionalJump(block, "CONDITION", false);

    BytecodeIndex ignored;
    compiler.compileSubstack(block, "SUBSTACK", block->nextBlock, ignored);

    compiler.patchJumpTarget(exitJump, compiler.here());
    return true;
}

SCRATCH_BLOCK(control, all_at_once) {
    const ParsedInput *input = Scratch::getInput(block, "SUBSTACK");
    if (input == nullptr) return BlockResult::CONTINUE;

    Block *substack = input->block;
    if (substack != nullptr) {
        thread->nextBlock = substack;
        return BlockResult::CONTINUE_IMMEDIATELY;
    }
    return BlockResult::CONTINUE;
}
SCRATCH_BYTECODE_BLOCK(control, all_at_once) {
    outFirstInstr = compiler.here();
    BytecodeIndex ignored;
    compiler.compileSubstack(block, "SUBSTACK", block->nextBlock, ignored);
    return true;
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
SCRATCH_BYTECODE_BLOCK(control, if_else) {
    outFirstInstr = compiler.here();
    BytecodeIndex elseJump = compiler.compileConditionalJump(block, "CONDITION", false);

    BytecodeIndex ignored;
    compiler.compileSubstack(block, "SUBSTACK", block->nextBlock, ignored);
    BytecodeIndex endJump = compiler.emit(BytecodeOp::Jump);

    compiler.patchJumpTarget(elseJump, compiler.here());
    compiler.compileSubstack(block, "SUBSTACK2", block->nextBlock, ignored);

    compiler.patchJumpTarget(endJump, compiler.here());
    return true;
}

SCRATCH_BLOCK_POSITIONAL_S_VOID(control, create_clone_of) {
    if (Scratch::cloneCount >= Scratch::maxClones) return;
    const std::string &input = str;

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

    if (!original) return;
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
    spriteToClone->costumeNameIndex = original->costumeNameIndex;
    spriteToClone->broadcasts = original->broadcasts;
    spriteToClone->renderInfo.forceUpdate = true;
    spriteToClone->hats = original->hats;

    Sprite *trueOriginal = original->isClone ? original->cloneOriginal : original;
    spriteToClone->cloneOriginal = trueOriginal;
    BytecodeExecutor::buildCloneLinkTable(spriteToClone, trueOriginal);
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
    return;
}
SCRATCH_BLOCK(control, create_clone_of) {
    if (Scratch::cloneCount >= Scratch::maxClones) return BlockResult::CONTINUE;
    std::string input;
    if (!Scratch::getInputValueAs(block, "CLONE_OPTION", thread, sprite, input)) return BlockResult::REPEAT;
    block_control_create_clone_of_pos_(sprite, input);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(control, create_clone_of, {"CLONE_OPTION", BytecodeStackKind::String});

SCRATCH_BLOCK(control, delete_this_clone) {
    if (!sprite->isClone) return BlockResult::CONTINUE;
    sprite->toDelete = true;
    for (ScriptThread *t : BlockExecutor::threads) {
        if (t->sprite == sprite) t->finished = true;
    }
    for (BytecodeThread *t : BytecodeExecutor::threads) {
        if (t->sprite == sprite) t->finished = true;
    }
    Scratch::cloneCount--;
    return BlockResult::RETURN;
}
SCRATCH_BLOCK_SIGNATURE(control, delete_this_clone);

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
        for (BytecodeThread *t : BytecodeExecutor::threads) {
            if (t->sprite->isClone) t->finished = true;
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
        for (BytecodeThread *t : BytecodeExecutor::threads) {
            if (t->sprite != sprite || t->id == thread->id || t->legacyThreadId == thread->id) continue;
            t->finished = true;
        }
        for (Sound sound : sprite->sounds)
            Mixer::stopSound(sound.fullName);
    }
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(control, stop, {"STOP_OPTION", BytecodeStackKind::String});
SCRATCH_BYTECODE_BLOCK(control, stop) {
    if (Scratch::getFieldValue(*block, "STOP_OPTION") != "this script") return false;

    outFirstInstr = compiler.here();
    compiler.emit(BytecodeOp::Halt);
    return true;
}

SCRATCH_BLOCK(control, start_as_clone) {
    if (!sprite->isClone) return BlockResult::CONTINUE;
    thread->finished = false;
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(control, start_as_clone);
SCRATCH_BYTECODE_BLOCK(control, start_as_clone) {
    outFirstInstr = compiler.here();
    return true;
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
SCRATCH_BYTECODE_BLOCK(control, forever) {
    BytecodeIndex loopStart = compiler.here();
    BytecodeIndex ignored;
    compiler.compileSubstack(block, "SUBSTACK", block, ignored);
    compiler.emit(BytecodeOp::Yield);
    compiler.emit(BytecodeOp::Jump, loopStart);
    outFirstInstr = loopStart;
    return true;
}

SCRATCH_BLOCK(control, wait_until) {
    bool condition;
    if (!Scratch::getInputValueAs(block, "CONDITION", thread, sprite, condition)) return BlockResult::REPEAT;

    if (condition) return BlockResult::CONTINUE;
    Scratch::resetInput(block);
    return BlockResult::REPEAT;
}
SCRATCH_BYTECODE_BLOCK(control, wait_until) {
    BytecodeIndex loopStart = compiler.here();
    BytecodeIndex exitJump = compiler.compileConditionalJump(block, "CONDITION", true);
    compiler.emit(BytecodeOp::Yield);
    compiler.emit(BytecodeOp::Jump, loopStart);
    compiler.patchJumpTarget(exitJump, compiler.here());
    outFirstInstr = loopStart;
    return true;
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
SCRATCH_BYTECODE_BLOCK(control, wait) {
    outFirstInstr = compiler.here();
    compiler.compileInput(block, "DURATION", BytecodeStackKind::Number);
    compiler.emit(BytecodeOp::Wait);
    return true;
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
    if (input == nullptr) return thread->withoutScreenRefresh ? BlockResult::CONTINUE_IMMEDIATELY : BlockResult::REPEAT;

    Block *substack = input->block;
    if (substack != nullptr)
        thread->nextBlock = substack;
    else return thread->withoutScreenRefresh ? BlockResult::CONTINUE_IMMEDIATELY : BlockResult::REPEAT;

    return BlockResult::CONTINUE_IMMEDIATELY;
}
SCRATCH_BYTECODE_BLOCK(control, repeat) {
    outFirstInstr = compiler.here();
    compiler.compileInput(block, "TIMES", BytecodeStackKind::Number);
    compiler.emit(BytecodeOp::ConvertNumberToValue);

    BytecodeIndex loopCheck = compiler.here();
    BytecodeIndex exitJump = compiler.emit(BytecodeOp::RepeatCheck);

    BytecodeIndex ignored;
    compiler.compileSubstack(block, "SUBSTACK", block, ignored);
    compiler.emit(BytecodeOp::Yield);
    compiler.emit(BytecodeOp::Jump, loopCheck);

    compiler.patchJumpTarget(exitJump, compiler.here());
    return true;
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
SCRATCH_BYTECODE_BLOCK(control, while) {
    BytecodeIndex loopStart = compiler.here();
    BytecodeIndex exitJump = compiler.compileConditionalJump(block, "CONDITION", false);

    BytecodeIndex ignored;
    compiler.compileSubstack(block, "SUBSTACK", block, ignored);
    compiler.emit(BytecodeOp::Yield);
    compiler.emit(BytecodeOp::Jump, loopStart);

    compiler.patchJumpTarget(exitJump, compiler.here());
    outFirstInstr = loopStart;
    return true;
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
SCRATCH_BYTECODE_BLOCK(control, repeat_until) {
    BytecodeIndex loopStart = compiler.here();
    BytecodeIndex exitJump = compiler.compileConditionalJump(block, "CONDITION", true);

    BytecodeIndex ignored;
    compiler.compileSubstack(block, "SUBSTACK", block, ignored);
    compiler.emit(BytecodeOp::Yield);
    compiler.emit(BytecodeOp::Jump, loopStart);

    compiler.patchJumpTarget(exitJump, compiler.here());
    outFirstInstr = loopStart;
    return true;
}

SCRATCH_BLOCK_POSITIONAL_SPRITE_N(control, get_counter) {
    return Scratch::counter;
}
SCRATCH_BLOCK_DOUBLE(control, get_counter) {
    *outValue = block_control_get_counter_pos_(sprite);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(control, get_counter);

SCRATCH_BLOCK_POSITIONAL_VOID(control, incr_counter) {
    Scratch::counter++;
}
SCRATCH_BLOCK(control, incr_counter) {
    block_control_incr_counter_pos_(sprite);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(control, incr_counter);

SCRATCH_BLOCK_POSITIONAL_VOID(control, clear_counter) {
    Scratch::counter = 0;
}
SCRATCH_BLOCK(control, clear_counter) {
    block_control_clear_counter_pos_(sprite);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(control, clear_counter);

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
SCRATCH_BYTECODE_BLOCK(control, for_each) {
    const std::string &varId = Scratch::getFieldId(*block, "VARIABLE");
    outFirstInstr = compiler.here();
    compiler.compileInput(block, "VALUE", BytecodeStackKind::Number);
    compiler.emit(BytecodeOp::ConvertNumberToValue);

    BytecodeIndex zeroIdx = static_cast<BytecodeIndex>(compiler.chunk().constants.size());
    compiler.chunk().constants.push_back(Value(0.0));
    compiler.emit(BytecodeOp::PushConstValue, zeroIdx);

    BytecodeIndex loopCheck = compiler.here();
    BytecodeIndex exitJump = compiler.emit(BytecodeOp::ForEachCheck, invalidBytecodeIndex, compiler.variableSlot(varId));

    BytecodeIndex ignored;
    compiler.compileSubstack(block, "SUBSTACK", block, ignored);
    compiler.emit(BytecodeOp::Yield);
    compiler.emit(BytecodeOp::Jump, loopCheck);

    compiler.patchJumpTarget(exitJump, compiler.here());
    return true;
}
