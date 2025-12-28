#pragma once
#include "text_gl2d.hpp"
#include <speech_text.hpp>
#include <string>
#include <vector>

class SpeechTextObjectGL2D : public TextObjectGL2D, public SpeechText {
  private:
    float measureTextWidth(const std::string &text) override;
    void platformSetText(const std::string &text) override;
    std::vector<std::string> splitTextByNewlines(const std::string &text);

  public:
    SpeechTextObjectGL2D(const std::string &text, int maxWidth = 100);
    ~SpeechTextObjectGL2D() override = default;

    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    std::vector<float> getSize() override;
};
