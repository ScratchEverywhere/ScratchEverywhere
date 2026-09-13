#include "speech_manager_c2d.hpp"
#include "render.hpp"

SpeechManagerC2D::SpeechManagerC2D() = default;

SpeechManagerC2D::~SpeechManagerC2D() {
    cleanup();
}

void SpeechManagerC2D::getScreenSize(int &outWidth, int &outHeight) {
    outWidth = Render::renderMode == Render::BOTH_SCREENS ? 400 : Render::getWidth();
    outHeight = Render::renderMode == Render::BOTH_SCREENS ? 480 : Render::getHeight();
}
