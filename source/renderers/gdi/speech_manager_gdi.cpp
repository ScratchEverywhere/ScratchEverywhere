#include "speech_manager_gdi.hpp"

SpeechManagerGDI::SpeechManagerGDI() = default;

SpeechManagerGDI::~SpeechManagerGDI() {
    cleanup();
}
