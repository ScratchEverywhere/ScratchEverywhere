#pragma once
#define WIN32_MEAN_AND_LEAN
#include <se_export.hpp>
#include <text.hpp>
#include <windows.h>

class SE_EXPORT TextObjectGDI : public TextObjectBase {
  private:
    HDC renderer = nullptr;

  protected:
    void uploadAtlas(FontGeneration &gen) override;

  public:
    TextObjectGDI(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectGDI() override;

    void render(int xPos, int yPos) override;
    void setRenderer(void *r) override;
};
