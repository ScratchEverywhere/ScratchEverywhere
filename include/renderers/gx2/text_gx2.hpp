#pragma once
#include <se_export.hpp>
#include <text.hpp>

class SE_EXPORT TextObjectGX2 : public TextObjectBase {
  protected:
    void uploadAtlas(FontGeneration &gen) override;

  public:
    TextObjectGX2(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectGX2() override = default;

    void render(int xPos, int yPos) override;
};
