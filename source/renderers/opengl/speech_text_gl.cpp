#include "speech_text_gl.hpp"
#include "text.hpp"
#include <os.hpp>

float SpeechTextObjectGL::measureTextWidth(const std::string &text) {
    return 0.0f;
}

void SpeechTextObjectGL::platformSetText(const std::string &text) {
}

SpeechTextObjectGL::SpeechTextObjectGL(const std::string &text, int maxWidth) : TextObjectGL(text, 0, 0), SpeechText(text, maxWidth) {
}

SpeechTextObjectGL::~SpeechTextObjectGL() {
}

void SpeechTextObjectGL::setText(std::string txt) {
}