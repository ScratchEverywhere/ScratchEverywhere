#pragma once
#include <se_export.hpp>
#include <text.hpp>

class SE_EXPORT TextObjectHeadless : public TextObject {
  public:
    TextObjectHeadless(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectHeadless() override;

    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    std::vector<float> getSize() override;
    std::vector<float> getStringSize(const std::string &txt) override;
};
