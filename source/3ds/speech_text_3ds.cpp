#include "speech_text_3ds.hpp"
#include "math.hpp"
#include <3ds.h>
#include <algorithm>

SpeechTextObject3DS::SpeechTextObject3DS(const std::string &text, int maxWidth)
    : TextObject3DS(text, 0, 0, "gfx/menu/Arialn"), SpeechText(text, maxWidth) {
    setColor(Math::color(0, 0, 0, 255));
    setCenterAligned(false); // easier for positioning logic
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
