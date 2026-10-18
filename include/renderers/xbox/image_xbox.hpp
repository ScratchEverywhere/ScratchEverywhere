#pragma once
#include <se_export.hpp>

#include "nonstd/expected.hpp"
#include <SDL3/SDL.h>
#include <image.hpp>
#include <string>
#include <vector>

class SE_EXPORT Image_Xbox : public Image {
  public:
    /**
     * A slice of a large image stored as its own (small) texture.
     * Used on Xbox, where one large contiguous texture allocation often fails
     * and swizzled textures get padded up to the next power of two.
     */
    struct Tile {
        SDL_Texture *texture = nullptr;
        int x = 0, y = 0, w = 0, h = 0; // source rect in image pixels
    };

  private:
    std::vector<Tile> tiles;

    nonstd::expected<void, std::string> setInitialTexture();
    nonstd::expected<void, std::string> buildTiles();
    void destroyTiles();
    void renderTiled(ImageRenderParams &params);

  public:
    SDL_Texture *texture = nullptr;
    SDL_Renderer *sdlRenderer = nullptr;

    Image_Xbox(std::string filePath, bool fromScratchProject = true, bool bitmapHalfQuality = false, float scale = 1);

    Image_Xbox(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality = false, float scale = 1);

    ~Image_Xbox() override;

    void render(ImageRenderParams &params) override;
    void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) override;

    void *getNativeTexture() override;

    nonstd::expected<void, std::string> refreshTexture() override;
};
