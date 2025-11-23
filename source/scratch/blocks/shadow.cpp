#include "blockUtils.hpp"
#include "interpret.hpp"

SCRATCH_SHADOW_BLOCK(matrix) {
    return Value(Scratch::getFieldValue(block, "MATRIX"));
}
