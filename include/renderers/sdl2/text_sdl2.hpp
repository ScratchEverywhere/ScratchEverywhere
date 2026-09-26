#pragma once
#include <SDL.h>
#include <se_export.hpp>
#include <text.hpp>

class SE_EXPORT TextObjectSDL2 : public TextObjectBase {
  private:
    SDL_Renderer *renderer = nullptr;

  protected:
    void uploadAtlas(FontGeneration &gen) override;

  public:
    TextObjectSDL2(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectSDL2() override;

    void render(int xPos, int yPos) override;
    void setRenderer(void *r) override;
};
