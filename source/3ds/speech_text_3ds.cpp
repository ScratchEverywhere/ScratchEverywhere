#include "speech_text_3ds.hpp"
#include "math.hpp"
#include <3ds.h>
#include <algorithm>

SpeechTextObject3DS::SpeechTextObject3DS(const std::string &text, int maxWidth)
    : TextObject3DS(text, 0, 0, "gfx/menu/Ubuntu-Bold"), maxWidth(maxWidth) {
    originalText = text;
    setColor(Math::color(0, 0, 0, 255));
    wrapText();
}

// Creates a temporary text object to measure its width then deletes it
float SpeechTextObject3DS::measureTextWidth(const std::string& text) {
    C2D_Text tempText;
    C2D_TextBuf tempBuffer = C2D_TextBufNew(200);
    C2D_TextFontParse(&tempText, *textClass.font, tempBuffer, text.c_str());
    C2D_TextOptimize(&tempText);
    
    float width, height;
    C2D_TextGetDimensions(&tempText, scale, scale, &width, &height);
    
    C2D_TextBufDelete(tempBuffer);
    return width;
}

void SpeechTextObject3DS::wrapText() {
    if (!textClass.font || originalText.empty()) {
        TextObject3DS::setText(originalText);
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

    TextObject3DS::setText(result);
}

void SpeechTextObject3DS::setText(std::string txt) {
    if (originalText == txt) return;
    originalText = txt;
    wrapText();
}
