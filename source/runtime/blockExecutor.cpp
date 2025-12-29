#include <blockExecutor.hpp>

Timer BlockExecutor::timer;

BlockResult BlockExecutor::runThreads() {
    for (auto &sprite : Runtime::sprites) {
        std::list<ScriptThread&> finishedThreads;
        auto it = sprite->threads.begin();
        while (it != sprite->threads.end()) {
            ScriptThread &thread = *it;
            BlockResult var = runThread(thread, *sprite);
                ++it;
            if (var.progress == Progress::RETURN_AND_STOP_SCRIPT) {
                it = sprite->threads.erase(it);
                finishedThreads.push_back(thread);
            } else if (var.progress == Progress::STOP_SPRITE) {
                return var;
            } else if (var.progress == Progress::CLOSE_PROJECT) {
                return var;
            }
        }
        //delete every finished thread:
        for (auto &thread : finishedThreads) {
            sprite->threads.remove(thread);
        }
    }
    return BlockResult();
}

BlockResult BlockExecutor::runThread(ScriptThread &thread, Sprite &sprite) {
    BlockResult var = Runtime::blocks[thread.currentBlock]->blockFunction(Runtime::blocks[thread.currentBlock].get(), &thread, &sprite);
    if (var.progress != Progress::CONTINUE) return var;
    if (var.next->blockId != 0) thread.currentBlock = var.next->blockId;
    return BlockResult(Value(), Progress::RETURN_AND_STOP_SCRIPT);
}

void BlockExecutor::runAllBlocksByOpcode(std::string opcode) {
    for (auto &sprite : Runtime::sprites) {
        for (auto &hat : sprite->hats) {
            if (Runtime::blocks[hat]->opcode == opcode) {
                sprite->startThread(hat);
            }
        }
    }
}
void BlockExecutor::runAllBlocksByOpcodeInSprite(std::string opcode, Sprite *sprite) {
    for (auto &hat : sprite->hats) {
        if (Runtime::blocks[hat]->opcode == opcode) {
            sprite->startThread(hat);
        }
    }
}