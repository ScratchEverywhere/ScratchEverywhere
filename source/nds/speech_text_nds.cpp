#include "speech_text_nds.hpp"
#include "../scratch/math.hpp"
#include "text_nds.hpp"
#include <string>
#include <vector>

SpeechTextObjectNDS::SpeechTextObjectNDS(const std::string &text, int maxWidth)
    : TextObjectNDS(text, 0, 0, "gfx/menu/Ubuntu-Bold", 14), SpeechText(text, maxWidth) {
    setColor(Math::color(0, 0, 0, 255));
    setCenterAligned(false);
    platformSetText(wrapText());
}

float SpeechTextObjectNDS::measureTextWidth(const std::string &text) {
    if (text.empty()) return 0.0f;

    TextObjectNDS temp("", 0, 0, "gfx/menu/Ubuntu-Bold", 14);
    temp.setText(text);
    auto size = temp.getSize();
    return size[0];
}

void SpeechTextObjectNDS::platformSetText(const std::string &text) {
    TextObjectNDS::setText(text);
}

void SpeechTextObjectNDS::setText(std::string txt) {
    SpeechText::setText(txt);
}

std::vector<float> SpeechTextObjectNDS::getSize() {
    std::vector<std::string> lines = splitTextByNewlines(getText());
    if (lines.empty()) {
        return {0.0f, 0.0f};
    }

    float maxWidth = 0.0f;
    float totalHeight = 0.0f;
    TextObjectNDS temp("", 0, 0, "gfx/menu/Ubuntu-Bold", 14);
    temp.setColor(color);
    temp.setCenterAligned(false);

    for (const auto &line : lines) {
        if (!line.empty()) {
            temp.setText(line);
            auto size = temp.getSize();
            if (size[0] > maxWidth) {
                maxWidth = size[0];
            }
            totalHeight += size[1];
        } else {
            temp.setText(" ");
            auto size = temp.getSize();
            totalHeight += size[1];
        }
    }

    return {maxWidth, totalHeight};
}

void SpeechTextObjectNDS::render(int xPos, int yPos) {
    std::vector<std::string> lines = splitTextByNewlines(getText());
    if (lines.empty()) return;

    TextObjectNDS temp("", 0, 0, "gfx/menu/Ubuntu-Bold", 14);
    temp.setColor(color);
    temp.setCenterAligned(false);

    float currentY = (float)yPos;

    for (const auto &line : lines) {
        if (!line.empty()) {
            temp.setText(line);
            auto size = temp.getSize();
            temp.render(xPos, (int)currentY);
            currentY += size[1];
        } else {
            temp.setText(" ");
            auto size = temp.getSize();
            currentY += size[1];
        }
    }
}

std::vector<std::string> SpeechTextObjectNDS::splitTextByNewlines(const std::string &text) {
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
    if (!currentLine.empty() || text.back() == '\n') {
        lines.push_back(currentLine);
    }

    return lines;
}
