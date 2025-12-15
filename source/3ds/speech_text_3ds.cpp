#include "speech_text_3ds.hpp"
#include "math.hpp"
#include <3ds.h>

SpeechTextObject3DS::SpeechTextObject3DS(const std::string &text, int maxWidth)
    : TextObject3DS(text, 0, 0, "gfx/menu/Ubuntu-Bold"), SpeechText(text, maxWidth) {
    setColor(Math::color(0, 0, 0, 255));
    setCenterAligned(false); // easier for positioning logic
    setScale(16.0f / 30.0f); // scale rasterised font from 30px to 16px
    platformSetText(wrapText());
}

// Creates a temporary text object to measure its width then deletes it
float SpeechTextObject3DS::measureTextWidth(const std::string &text) {
    C2D_Text tempText;
    C2D_TextBuf tempBuffer = C2D_TextBufNew(200);
    C2D_TextFontParse(&tempText, *textClass.font, tempBuffer, text.c_str());
    C2D_TextOptimize(&tempText);

    float width, height;
    C2D_TextGetDimensions(&tempText, scale, scale, &width, &height);

    C2D_TextBufDelete(tempBuffer);
    return width;
}

void SpeechTextObject3DS::platformSetText(const std::string &text) {
    TextObject3DS::setText(text);
}

void SpeechTextObject3DS::setText(std::string txt) {
    SpeechText::setText(txt);
}

std::vector<float> SpeechTextObject3DS::getSize() {
    float width, height;
    C2D_TextGetDimensions(&textClass.c2dText, scale, scale, &width, &height);
    return {width, height};
}

void SpeechTextObject3DS::render(int xPos, int yPos) {
    u32 flags = C2D_WithColor;
    C2D_DrawText(&textClass.c2dText, flags, xPos, yPos, 0, scale, scale, color);
}
