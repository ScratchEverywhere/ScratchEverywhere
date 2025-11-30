#include "blockUtils.hpp"

SCRATCH_BLOCK_NOP(coreExample, exampleWithInlineImage)

SCRATCH_REPORTER_BLOCK(coreExample, exampleOpcode) {
    return Value("Stage");
}
