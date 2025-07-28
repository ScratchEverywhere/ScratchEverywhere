#pragma once
#include "../scratch/text.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>

class TextObjectSDL : public TextObject {
  private:
    TTF_Font *font = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *texture = nullptr;
    int textWidth = 0;
    int textHeight = 0;

    void updateTexture();

  public:
    TextObjectSDL(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectSDL() override;

    void setColor(int clr) override;
    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    std::vector<float> getSize() override;
    void setRenderer(void *r) override;
};
