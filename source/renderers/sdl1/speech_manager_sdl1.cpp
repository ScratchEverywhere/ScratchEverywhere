#include "speech_manager_sdl1.hpp"

SpeechManagerSDL1::SpeechManagerSDL1() = default;

SpeechManagerSDL1::~SpeechManagerSDL1() {
    cleanup();
}
