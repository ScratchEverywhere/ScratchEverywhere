#include "text_headless.hpp"

HeadlessText::HeadlessText(std::string txt, double posX, double posY, std::string fontPath)
    : TextObject(txt, posX, posY, fontPath) {
}

HeadlessText::~HeadlessText() {
}

void HeadlessText::setText(std::string txt) {
    text = txt;
}

void HeadlessText::render(int xPos, int yPos) {
}

std::vector<float> HeadlessText::getSize() {
    return {0.0f, 0.0f};
}