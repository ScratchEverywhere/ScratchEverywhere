#pragma once
#include "../scratch/text.hpp"
#include <iostream>
#include <unordered_map>

#ifdef XBOX
#include <SDL.h>
#include <SDL_ttf.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#endif

class TextObjectSDL : public TextObject {
  private:
    static std::unordered_map<std::string, TTF_Font *> fonts;
    TTF_Font *font = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *texture = nullptr;
    size_t memorySize = 0;
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
