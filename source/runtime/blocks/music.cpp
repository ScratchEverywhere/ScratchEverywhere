#include "../../audiostack.hpp"
#include "blockUtils.hpp"
#include "runtime.hpp"

SCRATCH_SHADOW_BLOCK(music_menu_DRUM, DRUM)
SCRATCH_SHADOW_BLOCK(music_menu_INSTRUMENT, INSTRUMENT)
SCRATCH_SHADOW_BLOCK(note, NOTE);

SCRATCH_BLOCK(music, setInstrument) {
    if (!Scratch::getInputValueAs(block, "INSTRUMENT", thread, sprite, sprite->instrument)) return BlockResult::REPEAT;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(music, playNoteForBeats) {
    double note, beats;
    BlockState *state;
    if (!Scratch::getInputValueAs(block, "NOTE", thread, sprite, note)) return BlockResult::REPEAT;
    if (!Scratch::getInputValueAs(block, "BEATS", thread, sprite, beats)) return BlockResult::REPEAT;

    state = thread->getState(block);

    Mixer::initMusic();
    if (state->completedSteps == 0) {
        if ((state->musicChannel = Mixer::note(sprite->instrument, note, sprite->volume / 100.0, beats)) == -1) {
            thread->eraseState(block);
            return BlockResult::CONTINUE;
        }

        state->completedSteps = 1;
        return BlockResult::REPEAT;
    } else if (state->completedSteps == 1) {
        if (Mixer::isInstrumentPlaying(state->musicChannel)) return BlockResult::REPEAT;
    }

    thread->eraseState(block);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(music, playDrumForBeats) {
    double drum, beats;
    BlockState *state;
    if (!Scratch::getInputValueAs(block, "DRUM", thread, sprite, drum)) return BlockResult::REPEAT;
    if (!Scratch::getInputValueAs(block, "BEATS", thread, sprite, beats)) return BlockResult::REPEAT;

    state = thread->getState(block);

    Mixer::initMusic();
    if (state->completedSteps == 0) {
        if ((state->musicChannel = Mixer::drum(drum, sprite->volume / 100.0, beats)) == -1) {
            thread->eraseState(block);
            return BlockResult::CONTINUE;
        }

        state->completedSteps = 1;
        return BlockResult::REPEAT;
    } else if (state->completedSteps == 1) {
        if (Mixer::isInstrumentPlaying(state->musicChannel)) return BlockResult::REPEAT;
    }

    thread->eraseState(block);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(music, getTempo) {
    *outValue = Value(Scratch::tempo);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(music, setTempo) {
    if (!Scratch::getInputValueAs(block, "TEMPO", thread, sprite, Scratch::tempo)) return BlockResult::REPEAT;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(music, changeTempo) {
    double tempo;
    if (!Scratch::getInputValueAs(block, "TEMPO", thread, sprite, tempo)) return BlockResult::REPEAT;

    Scratch::tempo += tempo;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(music, restForBeats) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps == 1) {
        if (state->waitTimer.hasElapsed(state->waitDuration)) {
            thread->eraseState(block);
            return BlockResult::CONTINUE;
        }
        return BlockResult::REPEAT;
    }

    double beats;
    if (!Scratch::getInputValueAs(block, "BEATS", thread, sprite, beats)) return BlockResult::REPEAT;
    state->waitDuration = Mixer::beatsToSec(beats) * 1000;

    state->waitTimer.start();
    Scratch::forceRedraw = true;
    state->completedSteps = 1;
    return BlockResult::REPEAT;
}
