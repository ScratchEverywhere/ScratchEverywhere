#include "text_playdate.hpp"

TextObjectPlaydate::TextObjectPlaydate(std::string txt, double posX, double posY, std::string fontPath)
    : TextObject(txt, posX, posY, fontPath) {
}

TextObjectPlaydate::~TextObjectPlaydate() {
}

void TextObjectPlaydate::setText(std::string txt) {
    text = txt;
}

void TextObjectPlaydate::render(int xPos, int yPos) {
}

std::vector<float> TextObjectPlaydate::getSize() {
    return {0.0f, 0.0f};
}
