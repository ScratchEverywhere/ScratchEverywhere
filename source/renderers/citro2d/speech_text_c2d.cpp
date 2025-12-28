#include "speech_text_c2d.hpp"
#include <3ds.h>
#include <math.hpp>

SpeechTextObjectC2D::SpeechTextObjectC2D(const std::string &text, int maxWidth)
    : TextObjectC2D(text, 0, 0, "gfx/menu/Ubuntu-Bold"), SpeechText(text, maxWidth) {
    setColor(Math::color(0, 0, 0, 255));
    setCenterAligned(false); // easier for positioning logic
    setScale(16.0f / 30.0f); // scale rasterised font from 30px to 16px
    platformSetText(wrapText());
}

// Creates a temporary text object to measure its width then deletes it
float SpeechTextObjectC2D::measureTextWidth(const std::string &text) {
    C2D_Text tempText;
    C2D_TextBuf tempBuffer = C2D_TextBufNew(200);
    C2D_TextFontParse(&tempText, *textClass.font, tempBuffer, text.c_str());
    C2D_TextOptimize(&tempText);

    float width, height;
    C2D_TextGetDimensions(&tempText, scale, scale, &width, &height);

    C2D_TextBufDelete(tempBuffer);
    return width;
}

void SpeechTextObjectC2D::platformSetText(const std::string &text) {
    TextObjectC2D::setText(text);
}

void SpeechTextObjectC2D::setText(std::string txt) {
    SpeechText::setText(txt);
}

std::vector<float> SpeechTextObjectC2D::getSize() {
    float width, height;
    C2D_TextGetDimensions(&textClass.c2dText, scale, scale, &width, &height);
    return {width, height};
}

void SpeechTextObjectC2D::render(int xPos, int yPos) {
    u32 flags = C2D_WithColor;
    C2D_DrawText(&textClass.c2dText, flags, xPos, yPos, 0, scale, scale, color);
}
