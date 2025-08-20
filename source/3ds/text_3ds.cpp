#include "text_3ds.hpp"
#include <3ds.h>

TextObject3DS::TextObject3DS(std::string txt, double posX, double posY, std::string fontPath)
    : TextObject(txt, posX, posY, fontPath) {
    x = posX;
    y = posY;
    textClass.textBuffer = C2D_TextBufNew(200);

    if (fontPath == "" || fontPath.substr(fontPath.find_last_of('.') + 1) != "bcfnt")
        textClass.font = C2D_FontLoadSystem(CFG_REGION_USA);
    else
        textClass.font = C2D_FontLoad(fontPath.c_str());
    setText(txt);
}

TextObject3DS::~TextObject3DS() {
    if (textClass.textBuffer) {
        C2D_TextBufDelete(textClass.textBuffer);
        textClass.textBuffer = nullptr;
    }
    if (textClass.font) {
        C2D_FontFree(textClass.font);
        textClass.font = nullptr;
    }
}

void TextObject3DS::setText(std::string txt) {

    C2D_TextBufClear(textClass.textBuffer);

    // set and optimize the text
    C2D_TextFontParse(&textClass.c2dText, textClass.font, textClass.textBuffer, txt.c_str());
    C2D_TextOptimize(&textClass.c2dText);

    scale = scale;
    text = txt;
}

std::vector<float> TextObject3DS::getSize() {
    float width, height;
    C2D_TextGetDimensions(&textClass.c2dText, scale, scale, &width, &height);
    return {width, height};
}

void TextObject3DS::render(int xPos, int yPos) {
    u32 flags = C2D_WithColor;
    if (centerAligned) {
        flags |= C2D_AlignCenter;
    }
    yPos -= getSize()[1] / 2;
    C2D_DrawText(&textClass.c2dText, flags, xPos, yPos, 0, scale, scale, color);
}