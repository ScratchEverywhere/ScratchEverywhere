#pragma once
#include <SDL3/SDL.h>
#include <se_export.hpp>
#include <text.hpp>

class SE_EXPORT TextObjectSDL3 : public TextObjectBase {
  private:
    SDL_Renderer *renderer = nullptr;

  protected:
    void uploadAtlas(FontGeneration &gen) override;

  public:
    TextObjectSDL3(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectSDL3() override;

    void render(int xPos, int yPos) override;
    void setRenderer(void *r) override;
};
