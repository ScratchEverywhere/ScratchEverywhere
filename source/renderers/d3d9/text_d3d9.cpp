#include "text_d3d9.hpp"
#include "image_d3d9.hpp"
#include "render_d3d9.hpp"
#include <vector>

TextObjectD3D9::TextObjectD3D9(std::string txt, double posX, double posY, std::string fontPath)
    : TextObjectBase(txt, posX, posY, fontPath, 30.0f) {
}

TextObjectD3D9::~TextObjectD3D9() {
}

void TextObjectD3D9::uploadAtlas(FontGeneration &gen) {
}

void TextObjectD3D9::render(int xPos, int yPos) {
}
