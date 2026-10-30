#include "blockUtils.hpp"

SCRATCH_BLOCK(coreExample, exampleOpcode) {
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(coreExample, exampleOpcode);
