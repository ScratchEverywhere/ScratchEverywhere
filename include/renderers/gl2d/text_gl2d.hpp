#pragma once
#include <gl2d.h>
#include <nds.h>
#include <se_export.hpp>
#include <text.hpp>

class SE_EXPORT TextObjectGL2D : public TextObjectBase {
  private:
    float minScale = 1.0f;

  protected:
    void uploadAtlas(FontGeneration &gen) override;

  public:
    TextObjectGL2D(std::string txt, double posX, double posY, std::string fontPath = "", int fontSize = 16);
    ~TextObjectGL2D() override = default;

    void render(int xPos, int yPos) override;
};
