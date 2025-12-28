#pragma once
#include "text_c2d.hpp"
#include <speech_text.hpp>
#include <string>

class SpeechTextObject3DS : public TextObject3DS, public SpeechText {
  private:
    float measureTextWidth(const std::string &text) override;
    void platformSetText(const std::string &text) override;

  public:
    SpeechTextObject3DS(const std::string &text, int maxWidth = 200);
    ~SpeechTextObject3DS() override = default;

    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    std::vector<float> getSize() override;
};
