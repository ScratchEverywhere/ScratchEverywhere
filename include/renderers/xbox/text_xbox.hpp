#pragma once
#include <SDL3/SDL.h>
#include <se_export.hpp>
#include <text.hpp>

class SE_EXPORT TextObjectXbox : public TextObjectBase {
  private:
    SDL_Renderer *renderer = nullptr;

  protected:
    void uploadAtlas(FontGeneration &gen) override;

  public:
    TextObjectXbox(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectXbox() override;

    void render(int xPos, int yPos) override;
};
