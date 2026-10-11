#pragma once
#define WIN32_MEAN_AND_LEAN
#include <se_export.hpp>
#include <text.hpp>
#include <windows.h>

class SE_EXPORT TextObjectD3D9 : public TextObjectBase {
  private:
    int atlasWidth;
    int atlasHeight;

  protected:
    void uploadAtlas(FontGeneration &gen) override;

  public:
    TextObjectD3D9(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectD3D9() override;

    void render(int xPos, int yPos) override;
};
