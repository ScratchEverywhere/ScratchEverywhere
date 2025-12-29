#pragma once
#include "text_sdl3.hpp"
#include <speech_text.hpp>
#include <string>

class SpeechTextObjectSDL3 : public TextObjectSDL3, public SpeechText {
  private:
    float measureTextWidth(const std::string &text) override;
    void platformSetText(const std::string &text) override;

  public:
    SpeechTextObjectSDL3(const std::string &text, int maxWidth = 200);
    ~SpeechTextObjectSDL3() override;

    void setText(std::string txt) override;
};
