#pragma once
#include <SDL.h>
#include <se_export.hpp>
#include <text.hpp>

class SE_EXPORT TextObjectVFlash : public TextObjectBase {
  private:
    SDL_Renderer *renderer = nullptr;

  protected:
    void uploadAtlas(FontGeneration &gen) override;

  public:
    TextObjectVFlash(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectVFlash() override;

    void render(int xPos, int yPos) override;
};
