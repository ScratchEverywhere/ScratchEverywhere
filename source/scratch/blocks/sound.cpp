#include "sound.hpp"

Value SoundBlocks::volume(Block &block, Sprite *sprite) {
    return Value(sprite->volume);
}