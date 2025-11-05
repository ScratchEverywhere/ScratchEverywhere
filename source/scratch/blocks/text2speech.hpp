#pragma once
#include "blockExecutor.hpp"

class SpeechBlocks {
  public:
    static BlockResult speakAndWait(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult setVoiceTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult setLanguageTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
};
