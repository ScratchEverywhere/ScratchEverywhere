#pragma once
#include <se_export.hpp>
#include "nonstd/expected.hpp"
#include <image.hpp>
#include "renderer_nv2a.hpp"
#include <string>
#include <vector>

class SE_EXPORT Image_NV2A : public Image {
  public:
    struct Tile {
        NV2ATexture *texture = nullptr;
        int x = 0, y = 0, w = 0, h = 0;
    };

  private:
    std::vector<Tile> tiles;

    nonstd::expected<void, std::string> setInitialTexture();
    nonstd::expected<void, std::string> buildTiles();
    void destroyTiles();
    void renderTiled(ImageRenderParams &params);

  public:
    NV2ATexture *texture = nullptr;
    Renderer_NV2A *nv2aRenderer = nullptr;

    Image_NV2A(std::string filePath, bool fromScratchProject = true, bool bitmapHalfQuality = false, float scale = 1);
    Image_NV2A(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality = false, float scale = 1);
    ~Image_NV2A() override;

    void render(ImageRenderParams &params) override;
    void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) override;

    void *getNativeTexture() override;

    std::string cachedAssetPath;

    static void precacheProjectTextures();
    static std::string getTextureCacheDir();
    static std::string getTextureCachePath(const std::string &assetName);
    bool loadFromCache(const std::string &cachePath);

    nonstd::expected<void, std::string> refreshTexture() override;
};
