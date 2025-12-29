#include "blockUtils.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <runtime.hpp>
#include <math.h>
#include <sprite.hpp>
#include <value.hpp>

SCRATCH_REPORTER_BLOCK(operator, add) {
    //behavior like in Scratch
    //Scratch tries to get the Result (BlockResult) of NUM1 and after that NUM2, that means if NUM1 needs to wait, NUM2 is not executed yet
    //If the blocks are nested, we still don't reload the nested blocks in every frame
    //instead, if an input value from any block is already complete, it is stored within the BlockState struct.
    BlockResult NUM1 = block.getInput("NUM1", thread, sprite);
    if (NUM1.progress != Progress::CONTINUE) return NUM1;
    BlockResult NUM2 = block.getInput("NUM2", thread, sprite);
    if (NUM2.progress != Progress::CONTINUE) return NUM2;

    return BlockResult(NUM1.value + NUM2.value, Progress::CONTINUE);
}