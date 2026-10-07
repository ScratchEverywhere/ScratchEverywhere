#pragma once
#include <SDL.h>
#include <se_export.hpp>
#include <text.hpp>

class SE_EXPORT TextObjectSDL1 : public TextObjectBase {
  private:
    SDL_Surface *renderer = nullptr;

  protected:
    void uploadAtlas(FontGeneration &gen) override;

  public:
    TextObjectSDL1(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectSDL1() override;

    void render(int xPos, int yPos) override;
    void setRenderer(void *r) override;
};
