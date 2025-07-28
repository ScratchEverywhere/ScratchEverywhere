#pragma once
#include "../blockExecutor.hpp"

class SoundBlocks {
  public:
    static BlockResult playSoundUntilDone(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult playSound(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult stopAllSounds(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult changeEffectBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult setEffectTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult clearSoundEffects(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult changeVolumeBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult setVolumeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static Value volume(Block &block, Sprite *sprite);
};