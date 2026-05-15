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

    Mixer::initMusic();
    if (state->completedSteps == 0) {
        if ((state->musicChannel = Mixer::note(sprite->instrument, note.asDouble(), sprite->volume / 100.0, beats.asDouble())) == -1) {
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
    Value drum, beats;
    BlockState *state;
    if (!Scratch::getInput(block, "DRUM", thread, sprite, drum)) return BlockResult::REPEAT;
    if (!Scratch::getInput(block, "BEATS", thread, sprite, beats)) return BlockResult::REPEAT;

    state = thread->getState(block);

    Mixer::initMusic();
    if (state->completedSteps == 0) {
        if ((state->musicChannel = Mixer::drum(drum.asDouble(), sprite->volume / 100.0, beats.asDouble())) == -1) {
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
    Value tempo;
    if (!Scratch::getInput(block, "TEMPO", thread, sprite, tempo)) return BlockResult::REPEAT;

    Scratch::tempo = tempo.asDouble();

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(music, changeTempo) {
    Value tempo;
    if (!Scratch::getInput(block, "TEMPO", thread, sprite, tempo)) return BlockResult::REPEAT;

    Scratch::tempo += tempo.asDouble();

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
    Value beats;
    if (!Scratch::getInput(block, "BEATS", thread, sprite, beats)) return BlockResult::REPEAT;
    state->waitDuration = Mixer::beatsToSec(beats.asDouble()) * 1000;

    state->waitTimer.start();
    Scratch::forceRedraw = true;
    state->completedSteps = 1;
    return BlockResult::REPEAT;
}
