#include <audio.hpp>
#include <audiostack.hpp>
#include <maxmod9.h>
#include <nds.h>

static mm_word streamingCallback(mm_word length, mm_addr dest, mm_stream_formats format) {
    Mixer::requestSound((short *)dest, length);
    return length;
}

void SoundPlayer::init() {
    mm_ds_system mmSys =
        {
            .mod_count = 0,
            .samp_count = 0,
            .mem_bank = 0,
            .fifo_channel = FIFO_MAXMOD};
    mmInit(&mmSys);

    mm_stream stream =
        {
            .sampling_rate = Mixer::rate,
            .buffer_length = 2048, // We may be able to get away with 1024
            .callback = streamingCallback,
            .format = MM_STREAM_16BIT_STEREO,
            .timer = MM_TIMER2,
            .manual = false,
        };
    mmStreamOpen(&stream);
}
void SoundPlayer::cleanupAudio() {
    mmStreamClose();
    Mixer::cleanupAudio();
}
void SoundPlayer::deinit() {
    cleanupAudio();
}
void SoundPlayer::flushAudio() {
}
