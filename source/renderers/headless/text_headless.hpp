#pragma once
#include <text.hpp>

class TextObjectHeadless : public TextObject {
  public:
    TextObjectHeadless(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectHeadless() override;

    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    std::vector<float> getSize() override;
};
