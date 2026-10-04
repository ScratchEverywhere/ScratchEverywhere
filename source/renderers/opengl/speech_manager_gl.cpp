#include "speech_manager_gl.hpp"

SpeechManagerGL::SpeechManagerGL() = default;

SpeechManagerGL::~SpeechManagerGL() {
    cleanup();
}
