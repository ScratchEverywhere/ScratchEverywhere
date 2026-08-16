#include "speech_text_gl_core.hpp"
#include "text_gl_core.hpp"
#include <log.hpp>
#include <os.hpp>
#include <stb_truetype.h>

SpeechTextObjectGLCore::SpeechTextObjectGLCore(const std::string &text, int maxWidth)
    : TextObjectGLCore(text, 0, 0, "gfx/ingame/fonts/NotoSans-Medium"),
      SpeechText(text, maxWidth) {
    setColor(0x000000FF);
    setCenterAligned(false);
    platformSetText(wrapText());
}

SpeechTextObjectGLCore::~SpeechTextObjectGLCore() {}

float SpeechTextObjectGLCore::measureTextWidth(const std::string &text) {
    if (!font) return 0.0f;

    std::string cur;
    float maxW = 0.0f;
    for (char c : text) {
        if (c == '\n') {
            float x = 0, y = 0;
            for (unsigned char ch : cur) {
                if (ch < font->firstChar || ch >= font->firstChar + font->numChars) ch = 'x';
                stbtt_aligned_quad q;
                stbtt_GetBakedQuad(font->charData, font->atlasWidth, font->atlasHeight,
                                   ch - font->firstChar, &x, &y, &q, 1);
            }
            if (x > maxW) maxW = x;
            cur.clear();
        } else {
            cur += c;
        }
    }
    {
        float x = 0, y = 0;
        for (unsigned char ch : cur) {
            if (ch < font->firstChar || ch >= font->firstChar + font->numChars) ch = 'x';
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(font->charData, font->atlasWidth, font->atlasHeight,
                               ch - font->firstChar, &x, &y, &q, 1);
        }
        if (x > maxW) maxW = x;
    }
    return maxW;
}

void SpeechTextObjectGLCore::platformSetText(const std::string &text) {
    TextObjectGLCore::setText(text);
}

void SpeechTextObjectGLCore::setText(std::string txt) {
    SpeechText::setText(txt);
}
