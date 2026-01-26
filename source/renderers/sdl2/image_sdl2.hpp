#pragma once
#include <SDL2/SDL.h>
#include <image.hpp>
#include <string>
#include <unordered_map>

class Image_SDL2 : public Image {
  public:
    SDL_Texture *texture;

    Image_SDL2(std::string filePath, bool fromScratchProject = true);

    Image_SDL2(std::string filePath, mz_zip_archive *zip);

    ~Image_SDL2() override;

    void render(ImageRenderParams &params) override;
    void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) override;
};