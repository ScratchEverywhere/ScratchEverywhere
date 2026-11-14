#include "text_flatland.hpp"

TextObjectFlatland::TextObjectFlatland(std::string txt, double posX, double posY, std::string fontPath)
    : TextObject(txt, posX, posY, fontPath) {
}

TextObjectFlatland::~TextObjectFlatland() = default;

void TextObjectFlatland::setText(std::string txt) {
    text = txt;
}

void TextObjectFlatland::render(int xPos, int yPos) {}

std::vector<float> TextObjectFlatland::getSize() {
    return {0.0f, 0.0f};
}

std::vector<float> TextObjectFlatland::getStringSize(const std::string &txt) {
    return {0.0f, 0.0f};
}
