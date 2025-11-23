#include "blockUtils.hpp"
#include "interpret.hpp"

namespace blocks::shadow {
SCRATCH_SHADOW_BLOCK(matrix) {
    return Value(Scratch::getFieldValue(block, "MATRIX"));
}
} // namespace blocks::shadow
