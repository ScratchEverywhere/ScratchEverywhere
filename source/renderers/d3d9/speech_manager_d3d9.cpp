#include "speech_manager_d3d9.hpp"

SpeechManagerD3D9::SpeechManagerD3D9() = default;

SpeechManagerD3D9::~SpeechManagerD3D9() {
    cleanup();
}
