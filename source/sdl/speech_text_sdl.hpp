#pragma once
#include "text_sdl.hpp"
#include <string>

class SpeechTextObjectSDL : public TextObjectSDL {
  private:
    std::string originalText;
    int maxWidth;

    void wrapText();

  public:
    SpeechTextObjectSDL(const std::string &text, int maxWidth = 200);
    ~SpeechTextObjectSDL() override = default;

    void setText(std::string txt) override;
};