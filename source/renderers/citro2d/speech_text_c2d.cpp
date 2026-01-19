#include "speech_text_c2d.hpp"
#include <3ds.h>
#include <math.hpp>
#include <sstream>

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
    std::string text = getText();
    std::istringstream iss(text);
    std::string line;

    float maxWidth = 0.0f;
    float totalHeight = 0.0f;

    while (std::getline(iss, line)) {
        if (!line.empty()) {
            C2D_Text tempText;
            C2D_TextBuf tempBuffer = C2D_TextBufNew(200);
            C2D_TextFontParse(&tempText, *textClass.font, tempBuffer, line.c_str());
            C2D_TextOptimize(&tempText);

            float width, height;
            C2D_TextGetDimensions(&tempText, scale, scale, &width, &height);
            if (width > maxWidth) {
                maxWidth = width;
            }
            totalHeight += height;

            C2D_TextBufDelete(tempBuffer);
        } else {
            // Empty line still has height
            C2D_Text tempText;
            C2D_TextBuf tempBuffer = C2D_TextBufNew(200);
            C2D_TextFontParse(&tempText, *textClass.font, tempBuffer, " ");
            C2D_TextOptimize(&tempText);

            float height;
            C2D_TextGetDimensions(&tempText, scale, scale, nullptr, &height);
            totalHeight += height;

            C2D_TextBufDelete(tempBuffer);
        }
    }

    return {maxWidth, totalHeight};
}

void SpeechTextObjectC2D::render(int xPos, int yPos) {
    std::vector<std::string> lines;
    std::string text = getText();
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }
    if (!text.empty() && text.back() == '\n') {
        lines.push_back("");
    }
    if (lines.empty()) return;

    // Calculate line heights
    std::vector<float> lineHeights;
    for (const auto &line : lines) {
        C2D_Text tempText;
        C2D_TextBuf tempBuffer = C2D_TextBufNew(200);
        C2D_TextFontParse(&tempText, *textClass.font, tempBuffer, line.empty() ? " " : line.c_str());
        C2D_TextOptimize(&tempText);

        float height;
        C2D_TextGetDimensions(&tempText, scale, scale, nullptr, &height);
        lineHeights.push_back(height);

        C2D_TextBufDelete(tempBuffer);
    }

    // Render from bottom to top, keeping bottom fixed at yPos
    float currentBottomY = static_cast<float>(yPos);
    u32 flags = C2D_WithColor;

    for (int i = lines.size() - 1; i >= 0; i--) {
        const auto &line = lines[i];
        float lineHeight = lineHeights[i];
        int renderY = static_cast<int>(currentBottomY - lineHeight);

        if (!line.empty()) {
            C2D_Text tempText;
            C2D_TextBuf tempBuffer = C2D_TextBufNew(200);
            C2D_TextFontParse(&tempText, *textClass.font, tempBuffer, line.c_str());
            C2D_TextOptimize(&tempText);
            C2D_DrawText(&tempText, flags, xPos, renderY, 0, scale, scale, color);
            C2D_TextBufDelete(tempBuffer);
        } else {
            // Render space for empty lines
            C2D_Text tempText;
            C2D_TextBuf tempBuffer = C2D_TextBufNew(200);
            C2D_TextFontParse(&tempText, *textClass.font, tempBuffer, " ");
            C2D_TextOptimize(&tempText);
            C2D_DrawText(&tempText, flags, xPos, renderY, 0, scale, scale, color);
            C2D_TextBufDelete(tempBuffer);
        }

        currentBottomY -= lineHeight;
    }
}
