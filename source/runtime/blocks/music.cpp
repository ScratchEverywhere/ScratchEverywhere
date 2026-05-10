#include "blockUtils.hpp"
#include "runtime.hpp"

SCRATCH_SHADOW_BLOCK(music_menu_DRUM, DRUM)
SCRATCH_SHADOW_BLOCK(music_menu_INSTRUMENT, INSTRUMENT)

SCRATCH_BLOCK(music, setInstrument) {
    Value instrument;
    if (!Scratch::getInput(block, "INSTRUMENT", thread, sprite, instrument)) return BlockResult::REPEAT;

    sprite->instrument = instrument.asString();

    return BlockResult::CONTINUE;
}
