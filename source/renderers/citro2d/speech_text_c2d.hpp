#pragma once
#include "text_c2d.hpp"
#include <speech_text.hpp>
#include <string>
#include <vector>

class SpeechTextObjectC2D : public TextObjectC2D, public SpeechText {
  private:
    float measureTextWidth(const std::string &text) override;
    void platformSetText(const std::string &text) override;

  public:
    SpeechTextObjectC2D(const std::string &text, int maxWidth = 200);
    ~SpeechTextObjectC2D() override = default;

    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    std::vector<float> getSize() override;
};
