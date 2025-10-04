#include "speech_text.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>

SpeechTextObject::SpeechTextObject(const std::string &text, int maxWidth)
    : TextObjectSDL(text, 0, 0), maxWidth(maxWidth) {
    originalText = text;
    setColor(0x00);
    wrapText();
}

void SpeechTextObject::wrapText() {
    if (!font || originalText.empty()) {
        TextObjectSDL::setText(originalText);
        return;
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
                int width, height;
                TTF_SizeUTF8(font, line.c_str(), &width, &height);

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
        int width, height;
        TTF_SizeUTF8(font, line.c_str(), &width, &height);

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

    TextObjectSDL::setText(result);
}

void SpeechTextObject::setText(std::string txt) {
    if (originalText == txt) return;
    originalText = txt;
    wrapText();
}