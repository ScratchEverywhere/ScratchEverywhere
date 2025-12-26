#pragma once
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <text.hpp>
#include <unordered_map>

class TextObjectSDL1 : public TextObject {
  private:
    static std::unordered_map<std::string, TTF_Font *> fonts;
    static std::unordered_map<std::string, size_t> fontUsageCount;
    std::vector<std::string> splitTextByNewlines(const std::string &text);
    std::string pathFont;
    TTF_Font *font = nullptr;
    SDL_Surface *renderer = nullptr;
    SDL_Surface *texture = nullptr;
    size_t memorySize = 0;
    int textWidth = 0;
    int textHeight = 0;

    void updateTexture();

  public:
    TextObjectSDL1(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectSDL1() override;

    void setColor(int clr) override;
    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    std::vector<float> getSize() override;
    void setRenderer(void *r) override;
    static void cleanupText();
};
