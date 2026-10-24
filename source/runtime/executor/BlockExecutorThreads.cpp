#include "blockExecutor.hpp"
#include "bytecode_executor.hpp"
#include <algorithm>
#include <cstddef>
#include <renderer.hpp>
#include <runtime.hpp>
#include <speech_manager.hpp>
#include <vector>

uint64_t BlockExecutor::nextThreadId() {
    static uint64_t id = 0;
    return ++id;
}

bool BlockExecutor::isThreadRunning(uint64_t id) {
    for (ScriptThread *t : threads) {
        if (t->id == id) return !t->finished;
    }
    for (BytecodeThread *t : BytecodeExecutor::threads) {
        if (t->id != id) continue;
        if (!t->escapedToLegacy) return !t->finished;
        for (ScriptThread *legacy : threads) {
            if (legacy->id == t->legacyThreadId) return !legacy->finished;
        }
        return false;
    }
    return false;
}

uint64_t BlockExecutor::startHat(Sprite *sprite, Block *hat) {
    if (BytecodeThread *t = BytecodeExecutor::startThread(sprite, hat, true)) {
        return t->id;
    }
    return BlockExecutor::startThread(sprite, hat)->id;
}

std::vector<uint64_t> BlockExecutor::startBroadcastHats(std::string broadcast) {
    std::transform(broadcast.begin(), broadcast.end(), broadcast.begin(), ::tolower);

    std::vector<uint64_t> startedThreads;
    for (auto &spr : Scratch::sprites) {
        if (spr->hats["event_whenbroadcastreceived"].empty()) continue;
        for (Block *hat : spr->hats["event_whenbroadcastreceived"]) {
            std::string broadcastOption = Scratch::getFieldValue(*hat, "BROADCAST_OPTION");
            std::transform(broadcastOption.begin(), broadcastOption.end(), broadcastOption.begin(), ::tolower);

            if (broadcastOption == broadcast) startedThreads.push_back(startHat(spr, hat));
        }
    }
    return startedThreads;
}

ScriptThread *BlockExecutor::startThread(Sprite *sprite, Block *block, bool shouldRestart) {
    if (sprite->name == "Benchmark") fprintf(stderr, "[STARTTHREAD] sprite=Benchmark block=%p shouldRestart=%d\n", (void *)block, shouldRestart);
    size_t restartThreadIndex = -1;
    for (size_t i = 0; i < threads.size(); i++) {
        if (threads[i]->blockHat == block && sprite == threads[i]->sprite) {
            if (shouldRestart) {
                restartThreadIndex = i;
                break;
            } else return nullptr;
        }
    }

    if (BytecodeExecutor::isRunning(sprite, block)) {
        if (!shouldRestart) return nullptr;
        BytecodeExecutor::stopThread(sprite, block);
    }

    ScriptThread *newThread = nullptr;

    if (Pools::threads.empty()) newThread = new ScriptThread();
    else {
        newThread = Pools::threads.back();
        Pools::threads.pop_back();
    }

    newThread->blockHat = block;
    newThread->nextBlock = block;
    newThread->parentThread = nullptr;
    newThread->finished = false;
    newThread->id = nextThreadId();
    newThread->sprite = sprite;

    if (restartThreadIndex == -1) {
        threads.push_back(newThread);
    } else {
        auto &originalThread = threads[restartThreadIndex];
        originalThread->clear();
        Pools::threads.push_back(originalThread);
        threads.erase(threads.begin() + restartThreadIndex);
        threads.push_back(newThread);
    }

    return newThread;
}

void BlockExecutor::runThreads() {
    size_t i = 0;
    while (i < threads.size()) {
        ScriptThread *thread = threads[i];
        BlockResult var;

        if (thread->finished) {
            thread->clear();
            Pools::threads.push_back(thread);
            threads.erase(threads.begin() + i);
            continue;
        }

        var = runThread(*thread, *thread->sprite);

        if (Scratch::shouldStop) return;
        i++;
    }

    Scratch::sprites.erase(
        std::remove_if(Scratch::sprites.begin(), Scratch::sprites.end(),
                       [](Sprite *s) {
                           if (s->toDelete) {
                               for (auto &thread : threads) {
                                   if (thread->sprite == s) {
                                       thread->finished = true;
                                   }
                               }
                               SpeechManager *speechManager = Render::getSpeechManager();
                               if (speechManager) speechManager->clearSpeech(s);
                               sortSprites = true;
                               delete s;
                               return true;
                           }
                           return false;
                       }),
        Scratch::sprites.end());

    if (sortSprites) {
        for (unsigned int i = 0; i < Scratch::sprites.size(); i++) {
            Scratch::sprites[i]->layer = (Scratch::sprites.size() - 1) - i;
        }
        sortSprites = false;
    }
    if (stopClicked) {
        Scratch::stopClicked();
    }
}

BlockResult BlockExecutor::runThread(ScriptThread &thread, Sprite &sprite) {
    if (thread.nextBlock == nullptr) return BlockResult::RETURN;
    BlockResult var = BlockResult::CONTINUE;
    Timer executionTimer(false);
    if (Scratch::warpTimer) executionTimer.start();
    Block *currentBlock = nullptr;
    unsigned int blocksSinceTimeCheck = 0;
    constexpr unsigned int timeCheckInterval = 64;
    do {
        currentBlock = thread.nextBlock;
        thread.nextBlock = currentBlock->nextBlock;

        switch (currentBlock->blockFunction.type) {
        case Type::Value: {
            var = currentBlock->blockFunction.func.value(currentBlock, &thread, &sprite, nullptr);
            break;
        }
        case Type::Number: {
            double blockOutDouble;
            var = currentBlock->blockFunction.func.number(currentBlock, &thread, &sprite, &blockOutDouble);
            break;
        }
        case Type::String: {
            std::string blockOutString;
            var = currentBlock->blockFunction.func.string(currentBlock, &thread, &sprite, &blockOutString);
            break;
        }
        case Type::Boolean: {
            bool blockOutBool;
            var = currentBlock->blockFunction.func.boolean(currentBlock, &thread, &sprite, &blockOutBool);
            break;
        }
        case Type::Color: {
            Color blockOutColor;
            var = currentBlock->blockFunction.func.color(currentBlock, &thread, &sprite, &blockOutColor);
            break;
        }
        }

        if (var == BlockResult::REPEAT) thread.nextBlock = currentBlock;
        else {
            Scratch::resetInput(currentBlock);
        }

        if (Scratch::warpTimer && thread.withoutScreenRefresh && ++blocksSinceTimeCheck >= timeCheckInterval) {
            blocksSinceTimeCheck = 0;
            if (executionTimer.getTimeMs() > 500) break;
        }

    } while ((var == BlockResult::CONTINUE_IMMEDIATELY || (var == BlockResult::CONTINUE && (!currentBlock->isEndBlock || thread.withoutScreenRefresh))) && !thread.finished && thread.nextBlock != nullptr && !Scratch::shouldStop);
    if (currentBlock == nullptr || var == BlockResult::RETURN || (var != BlockResult::REPEAT && currentBlock->nextBlock == nullptr)) thread.finished = true;
    return var;
}

void BlockExecutor::runAllBlocksByOpcode(const std::string &opcode, std::vector<ScriptThread *> *out) {
    for (auto *sprite : Scratch::sprites) {
        runAllBlocksByOpcodeInSprite(opcode, sprite);
    }
}

void BlockExecutor::runAllBlocksByOpcodeInSprite(const std::string &opcode, Sprite *sprite, std::vector<ScriptThread *> *out) {
    if (sprite->hats[opcode].empty()) return;
    std::vector<Block *> tempHats(sprite->hats[opcode].begin(), sprite->hats[opcode].end());

    for (auto it = tempHats.rbegin(); it != tempHats.rend(); ++it) {
        auto &hat = *it;

        if (out == nullptr && BytecodeExecutor::startThread(sprite, hat, true) != nullptr) continue;

        ScriptThread *thread = BlockExecutor::startThread(sprite, hat);
        if (out) out->push_back(thread);
    }
}
