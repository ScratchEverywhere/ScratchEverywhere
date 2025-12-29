#include "blockUtils.hpp"

BlockResult nopBlock(Block &block, ScriptThread *thread, Sprite *sprite) {
    return BlockResult(Value(), Progress::CONTINUE);
}
