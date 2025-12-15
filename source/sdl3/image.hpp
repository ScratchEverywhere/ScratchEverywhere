#pragma once

#include <SDL3_image/SDL_image.h>
#include <string>
#include <unordered_map>

class SDL_Image {
  public:
    size_t imageUsageCount = 0;
    SDL_Surface *spriteSurface;
    SDL_Texture *spriteTexture;
    SDL_FRect renderRect;  // this rect is for rendering to the screen
    SDL_FRect textureRect; // this is for like texture UV's
    size_t memorySize;
    float scale = 1.0f;
    float width;
    float height;
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
     * @param filePath
     */
    SDL_Image(std::string filePath, bool fromScratchProject = true);

    ~SDL_Image();
};

extern std::unordered_map<std::string, SDL_Image *> images;
