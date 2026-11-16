#include "speech_manager_gl_core.hpp"

SpeechManagerGLCore::SpeechManagerGLCore() = default;

SpeechManagerGLCore::~SpeechManagerGLCore() {
    cleanup();
}
