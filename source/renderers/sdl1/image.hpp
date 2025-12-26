#pragma once

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <string>
#include <unordered_map>

class SDL_Image {
  public:
    size_t imageUsageCount = 0;
    SDL_Surface *spriteSurface;
    SDL_Surface *spriteTexture;
    SDL_Rect renderRect;  // this rect is for rendering to the screen
    SDL_Rect textureRect; // this is for like texture UV's
    size_t memorySize;
    float scale = 1.0f;
    int width;
    int height;
    float rotation = 0.0f;
#ifdef GAMECUBE
    int maxFreeTime = 2;
#else
    int maxFreeTime = 480;
#endif

    int freeTimer = maxFreeTime;

    /**
     * Scales an image by a scale factor.
     * @param scaleAmount
     */
    void setScale(float amount);
    /**
     * Sets Image rotation (in radians)
     * @param rotationAmount In radians
     */
    void setRotation(float amount);

    /**
     * A Simple Image object using SDL.
     */
    SDL_Image();
    /**
     * A Simple Image object using SDL.
     * @param filePath
     */
    SDL_Image(std::string filePath, bool fromScratchProject = true);

    ~SDL_Image();
};

SDL_Surface *SVGToSurface(const char *svg_data, size_t svg_size);

extern std::unordered_map<std::string, SDL_Image *> images;
