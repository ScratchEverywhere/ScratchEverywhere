#pragma once
#include "../blockExecutor.hpp"

class SoundBlocks {
  public:
    static BlockResult playSoundUntilDone(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult playSound(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult stopAllSounds(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult changeEffectBy(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult setEffectTo(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult clearSoundEffects(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult changeVolumeBy(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static BlockResult setVolumeTo(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh);
    static Value volume(Block &block, Sprite *sprite);
};