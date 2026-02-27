#include "speech_text_gl.hpp"
#include "text.hpp"
#include "text_gl.hpp"
#include <os.hpp>
#include <stb_truetype.h>

SpeechTextObjectGL::SpeechTextObjectGL(const std::string &text, int maxWidth)
    : TextObjectGL(text, 0, 0, "gfx/menu/LibSansN"), SpeechText(text, maxWidth) {
    setColor(0x000000FF);
    setCenterAligned(false); // easier for positioning logic
    platformSetText(wrapText());
}

SpeechTextObjectGL::~SpeechTextObjectGL() {
}

float SpeechTextObjectGL::measureTextWidth(const std::string &text) {
    if (!font) return 0.0f;

    std::vector<std::string> lines;
    std::string currentLine;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(currentLine);
            currentLine.clear();
        } else {
            currentLine += c;
        }
    }
    lines.push_back(currentLine);

    float maxWidth = 0.0f;
    for (const auto &line : lines) {
        float x = 0, y = 0;
        for (unsigned char c : line) {
            if (c < font->firstChar || c >= font->firstChar + font->numChars) {
                c = 'x';
            }

            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(font->charData, font->atlasWidth, font->atlasHeight, c - font->firstChar, &x, &y, &q, 1);
        }
        if (x > maxWidth) maxWidth = x;
    }

    return maxWidth;
}

void SpeechTextObjectGL::platformSetText(const std::string &text) {
    TextObjectGL::setText(text);
}

void SpeechTextObjectGL::setText(std::string txt) {
    SpeechText::setText(txt);
}