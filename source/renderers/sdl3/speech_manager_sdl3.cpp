#include "speech_manager_sdl3.hpp"

SpeechManagerSDL3::SpeechManagerSDL3() = default;

SpeechManagerSDL3::~SpeechManagerSDL3() {
    cleanup();
}
