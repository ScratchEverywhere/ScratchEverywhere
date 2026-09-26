#pragma once
#include <se_export.hpp>
#include <text.hpp>

class SE_EXPORT TextObjectGL : public TextObjectBase {
  protected:
    void uploadAtlas(FontGeneration &gen) override;

  public:
    TextObjectGL(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectGL() override = default;

    void render(int xPos, int yPos) override;
};
