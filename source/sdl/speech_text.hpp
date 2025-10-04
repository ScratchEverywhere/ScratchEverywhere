#pragma once
#include "text_sdl.hpp"
#include <string>

class SpeechTextObject : public TextObjectSDL {
private:
    std::string originalText;
    int maxWidth;

    void wrapText();

public:
    SpeechTextObject(const std::string& text, int maxWidth = 200);
    ~SpeechTextObject() override = default;

    void setText(std::string txt) override;
};
