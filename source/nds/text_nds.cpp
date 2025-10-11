#include "text_nds.hpp"

TextObjectNDS::TextObjectNDS(std::string txt, double posX, double posY, std::string fontPath)
    : TextObject(txt, posX, posY, fontPath) {
}

TextObjectNDS::~TextObjectNDS() {
}

void TextObjectNDS::setText(std::string txt) {
    text = txt;
}

void TextObjectNDS::render(int xPos, int yPos) {
}

std::vector<float> TextObjectNDS::getSize() {
    return {0.0f, 0.0f};
}

void TextObjectNDS::cleanupText() {
}