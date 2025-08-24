#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <unordered_map>

class SDL_Image {
  public:
    SDL_Surface *spriteSurface;
    SDL_Texture *spriteTexture;
    SDL_Rect renderRect;  // this rect is for rendering to the screen
    SDL_Rect textureRect; // this is for like texture UV's
    float scale = 1.0f;
    int width;
    int height;
    float rotation = 0.0f;

    int freeTimer = 120;
    void setScale(float amount);
    void setRotation(float amount);
    SDL_Image();
    SDL_Image(std::string filePath);
    ~SDL_Image();
};

extern std::unordered_map<std::string, SDL_Image *> images;