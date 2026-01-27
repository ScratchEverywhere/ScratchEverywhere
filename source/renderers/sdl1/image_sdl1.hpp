#pragma once

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <image.hpp>
#include <string>
#include <unordered_map>

class Image_SDL1 : public Image {
  private:
    void setInitialTexture();

  public:
    SDL_Surface *texture;

    Image_SDL1(std::string filePath, bool fromScratchProject = true);

    Image_SDL1(std::string filePath, mz_zip_archive *zip);

    ~Image_SDL1() override;

    void render(ImageRenderParams &params) override;
    void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) override;
};