#pragma once
#define WIN32_MEAN_AND_LEAN
#include <se_export.hpp>
#include <text.hpp>
#include <windows.h>

class SE_EXPORT TextObjectGDI : public TextObjectBase {
  private:
    HDC hDC;
    HBITMAP hBitmap;
    RGBQUAD *quad;
    int atlasWidth;
    int atlasHeight;

  protected:
    void uploadAtlas(FontGeneration &gen) override;

  public:
    TextObjectGDI(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectGDI() override;

    void render(int xPos, int yPos) override;
};
