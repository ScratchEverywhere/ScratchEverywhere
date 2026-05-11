#include "../../audiostack.hpp"
#include "blockUtils.hpp"
#include "runtime.hpp"

SCRATCH_SHADOW_BLOCK(music_menu_DRUM, DRUM)
SCRATCH_SHADOW_BLOCK(music_menu_INSTRUMENT, INSTRUMENT)
SCRATCH_SHADOW_BLOCK(note, NOTE);

SCRATCH_BLOCK(music, setInstrument) {
    Value instrument;
    if (!Scratch::getInput(block, "INSTRUMENT", thread, sprite, instrument)) return BlockResult::REPEAT;

    sprite->instrument = instrument.asDouble();

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(music, playNoteForBeats) {
    Value note, beats;
    BlockState *state;
    if (!Scratch::getInput(block, "NOTE", thread, sprite, note)) return BlockResult::REPEAT;
    if (!Scratch::getInput(block, "BEATS", thread, sprite, beats)) return BlockResult::REPEAT;

    state = thread->getState(block);

    if (state->completedSteps == 0) {
        if ((state->musicChannel = Mixer::note(sprite->instrument, note.asDouble(), sprite->volume / 100.0, beats.asDouble())) == -1) {
            thread->eraseState(block);
            return BlockResult::CONTINUE;
        }

        state->completedSteps = 1;
        return BlockResult::REPEAT;
    } else if (state->completedSteps == 1) {
        if (Mixer::isNotePlaying(state->musicChannel)) return BlockResult::REPEAT;
    }

    thread->eraseState(block);

    return BlockResult::CONTINUE;
}
