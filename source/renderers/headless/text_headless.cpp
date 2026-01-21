#include "text_headless.hpp"

TextObjectHeadless::TextObjectHeadless(std::string txt, double posX, double posY, std::string fontPath)
    : TextObject(txt, posX, posY, fontPath) {
}

TextObjectHeadless::~TextObjectHeadless() {
}

void TextObjectHeadless::setText(std::string txt) {
    text = txt;
}

void TextObjectHeadless::render(int xPos, int yPos) {
}

std::vector<float> TextObjectHeadless::getSize() {
    return {0.0f, 0.0f};
}
