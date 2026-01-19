#pragma once
#include <SDL2/SDL.h>
#include <SDL_ttf.h>
#include <text.hpp>
#include <unordered_map>

class TextObjectSDL2 : public TextObject {
  private:
    std::vector<std::string> splitTextByNewlines(const std::string &text);
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *texture = nullptr;
    size_t memorySize = 0;
    int textWidth = 0;
    int textHeight = 0;

    void updateTexture();

  protected:
    static std::unordered_map<std::string, TTF_Font *> fonts;
    static std::unordered_map<std::string, size_t> fontUsageCount;
    std::string pathFont;
    TTF_Font *font = nullptr;

  public:
    TextObjectSDL2(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectSDL2() override;

    void setColor(int clr) override;
    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    std::vector<float> getSize() override;
    void setRenderer(void *r) override;
    static void cleanupText();
};
