#pragma once
#include "blockUtils.hpp"

namespace blocks::sound {
BlockResult playSoundUntilDone(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult playSound(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult stopAllSounds(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult changeEffectBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult setEffectTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult clearSoundEffects(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult changeVolumeBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult setVolumeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
Value volume(Block &block, Sprite *sprite);
} // namespace blocks::sound
