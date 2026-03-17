#pragma once
#include <text.hpp>

class TextObjectPlaydate : public TextObject {
  public:
    TextObjectPlaydate(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectPlaydate() override;

    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    std::vector<float> getSize() override;
};
