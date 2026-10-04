#include "text_gdi.hpp"
#include <vector>

TextObjectGDI::TextObjectGDI(std::string txt, double posX, double posY, std::string fontPath)
    : TextObjectBase(txt, posX, posY, fontPath, 30.0f) {
}

TextObjectGDI::~TextObjectGDI() = default;

void TextObjectGDI::setRenderer(void *r) {
}

void TextObjectGDI::uploadAtlas(FontGeneration &gen) {
}

void TextObjectGDI::render(int xPos, int yPos) {
}
