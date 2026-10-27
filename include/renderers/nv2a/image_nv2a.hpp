#pragma once
#include <se_export.hpp>
#include "nonstd/expected.hpp"
#include <image.hpp>
#include "renderer_nv2a.hpp"
#include <string>
#include <vector>

#include <cmath>
#include <algorithm>

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
    void renderMosaic(ImageRenderParams &params, NV2ATexture *activeTex);
    void renderPixelate(ImageRenderParams &params, NV2ATexture *activeTex);
    void renderDeformedMesh(ImageRenderParams &params, NV2ATexture *activeTex,
                            bool hasWhirl, bool hasFisheye, bool hasMosaic);

    // Looks effects (color/fisheye/whirl/pixelate/mosaic) are evaluated on the CPU into cached textures.
    struct EffectCacheSlot {
        NV2ATexture *texture = nullptr;
        float color = 0.0f;
        float fisheye = 0.0f;
        float whirl = 0.0f;
        float pixelate = 0.0f;
        float mosaic = 0.0f;
        uint32_t lastUsedFrame = 0;

        bool matches(const ImageRenderParams &p) const {
            return std::abs(color - p.colorEffect) < 0.001f &&
                   std::abs(pixelate - p.pixelateEffect) < 0.001f;
        }
    };
    EffectCacheSlot effectSlots[2];
    std::vector<uint8_t> effectSource;
    const uint8_t *getEffectSource(int &pitch);
    NV2ATexture *acquireEffectTexture(const ImageRenderParams &params);
    void releaseEffects();

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
