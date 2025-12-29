#include "blockUtils.hpp"
#include <input.hpp>
#include <runtime.hpp>
#include <sprite.hpp>

SCRATCH_BLOCK_NOP(event, whenflagclicked) {
        //search for a thread in threads.blockHatID thats the same as block.blockId
    auto &threads = sprite->threads;
    for (auto &t : threads) {
        if (t.blockHatID == block.blockId) {
            t.finished = true;
        }
    }

    return BlockResult(Value(), Progress::CONTINUE);
}
