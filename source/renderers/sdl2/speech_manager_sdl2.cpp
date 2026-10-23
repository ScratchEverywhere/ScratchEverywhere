#include "speech_manager_sdl2.hpp"

SpeechManagerSDL2::SpeechManagerSDL2() = default;

SpeechManagerSDL2::~SpeechManagerSDL2() {
    cleanup();
}
