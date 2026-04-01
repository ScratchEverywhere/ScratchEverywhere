#include <algorithm>
#include <runtime/blocks/blockUtils.hpp>

SCRATCH_REPORTER_BLOCK(utilities, clamp) {
    const Value input = Scratch::getInputValue(block, "INPUT", sprite);
    const Value min = Scratch::getInputValue(block, "MIN", sprite);
    const Value max = Scratch::getInputValue(block, "MAX", sprite);

    return Value(std::clamp(input.asDouble(), min.asDouble(), max.asDouble()));
}
