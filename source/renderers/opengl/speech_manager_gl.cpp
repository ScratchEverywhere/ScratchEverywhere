#include "speech_manager_gl.hpp"
#include "image.hpp"
#include "render.hpp"
#include <image.hpp>
#include <render.hpp>
#include <runtime.hpp>

double SpeechManagerGL::getCurrentTime() {
    return 0.0;
}

void SpeechManagerGL::createSpeechObject(Sprite *sprite, const std::string &message) {
}

void SpeechManagerGL::renderSpeechIndicator(Sprite *sprite, int spriteCenterX, int spriteCenterY, int spriteTop, int spriteLeft, int spriteRight, int bubbleX, int bubbleY, int bubbleWidth, int bubbleHeight, double scale) {
}

void SpeechManagerGL::ensureImagesLoaded() {
}

SpeechManagerGL::SpeechManagerGL(/* some opengl window idk */) {
}

SpeechManagerGL::~SpeechManagerGL() {
}

void SpeechManagerGL::render() {
}