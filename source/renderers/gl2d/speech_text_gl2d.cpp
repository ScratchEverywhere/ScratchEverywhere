#include "speech_text_gl2d.hpp"
#include "text_gl2d.hpp"
#include <math.hpp>

SpeechTextObjectGL2D::SpeechTextObjectGL2D(const std::string &text, int maxWidth)
    : TextObjectGL2D(text, 0, 0, "gfx/menu/Ubuntu-Bold", 14), SpeechText(text, maxWidth) {
    setColor(Math::color(0, 0, 0, 255));
    setCenterAligned(false);
    platformSetText(wrapText());
}

float SpeechTextObjectGL2D::measureTextWidth(const std::string &text) {
    if (text.empty()) return 0.0f;

    TextObjectGL2D temp("", 0, 0, "gfx/menu/Ubuntu-Bold", 14);
    temp.setText(text);
    auto size = temp.getSize();
    return size[0];
}

void SpeechTextObjectGL2D::platformSetText(const std::string &text) {
    TextObjectGL2D::setText(text);
}

void SpeechTextObjectGL2D::setText(std::string txt) {
    SpeechText::setText(txt);
}

std::vector<float> SpeechTextObjectGL2D::getSize() {
    std::vector<std::string> lines = splitTextByNewlines(getText());
    if (lines.empty()) {
        return {0.0f, 0.0f};
    }

    float maxWidth = 0.0f;
    float totalHeight = 0.0f;
    TextObjectGL2D temp("", 0, 0, "gfx/menu/Ubuntu-Bold", 14);
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

void SpeechTextObjectGL2D::render(int xPos, int yPos) {
    std::vector<std::string> lines = splitTextByNewlines(getText());
    if (lines.empty()) return;

    TextObjectGL2D temp("", 0, 0, "gfx/menu/Ubuntu-Bold", 14);
    temp.setColor(color);
    temp.setCenterAligned(false);

    std::vector<float> lineHeights;
    for (const auto &line : lines) {
        if (!line.empty()) {
            temp.setText(line);
            auto size = temp.getSize();
            lineHeights.push_back(size[1]);
        } else {
            temp.setText(" ");
            auto size = temp.getSize();
            lineHeights.push_back(size[1]);
        }
    }

    float totalHeight = 0.0f;
    for (float h : lineHeights) {
        totalHeight += h;
    }

    // Render from bottom to top, keeping bottom fixed at yPos
    float currentBottomY = (float)yPos;
    for (int i = lines.size() - 1; i >= 0; i--) {
        const auto &line = lines[i];
        float lineHeight = lineHeights[i];
        int renderY = (int)(currentBottomY - lineHeight);

        if (!line.empty()) {
            temp.setText(line);
            temp.render(xPos, renderY);
        } else {
            temp.setText(" ");
            temp.render(xPos, renderY);
        }

        currentBottomY -= lineHeight;
    }
}

std::vector<std::string> SpeechTextObjectGL2D::splitTextByNewlines(const std::string &text) {
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
