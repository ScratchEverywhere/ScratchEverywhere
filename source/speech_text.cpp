#include "speech_text.hpp"

SpeechText::SpeechText(const std::string &text, int maxWidth)
    : maxWidth(maxWidth) {
    originalText = text;
}

std::string SpeechText::wrapText() {
    if (originalText.empty()) {
        return originalText;
    }

    std::string result;
    std::string currentLine;
    std::string currentWord;

    for (char c : originalText) {
        if (c == '\n') {
            if (!currentWord.empty()) {
                currentLine += currentWord;
                currentWord.clear();
            }
            if (!currentLine.empty()) {
                result += currentLine + "\n";
                currentLine.clear();
            } else {
                result += "\n";
            }
        } else if (c == ' ') { // add new line at space to wrap cleanly (without splitting words in half)
            if (!currentWord.empty()) {
                std::string line = currentLine.empty() ? currentWord : currentLine + " " + currentWord;

                float width = measureTextWidth(line);

                if (width <= maxWidth) {
                    currentLine = line;
                } else {
                    if (!currentLine.empty()) {
                        result += currentLine + "\n";
                    }
                    currentLine = currentWord;
                }
                currentWord.clear();
            }
        } else {
            currentWord += c;
        }
    }

    // Handle the last word
    if (!currentWord.empty()) {
        std::string line = currentLine.empty() ? currentWord : currentLine + " " + currentWord;

        float width = measureTextWidth(line);

        if (width <= maxWidth) {
            currentLine = line;
        } else {
            if (!currentLine.empty()) {
                result += currentLine + "\n";
            }
            currentLine = currentWord;
        }
    }

    if (!currentLine.empty()) {
        result += currentLine;
    }

    return result;
}

void SpeechText::setText(const std::string &txt) {
    if (originalText == txt) return;
    originalText = txt;
    platformSetText(wrapText());
}
