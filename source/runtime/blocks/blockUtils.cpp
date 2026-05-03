#include "blockUtils.hpp"

BlockResult nopBlock(Block *block, ScriptThread *thread, Sprite *sprite, Value *outValue) {
    return BlockResult::CONTINUE;
}
