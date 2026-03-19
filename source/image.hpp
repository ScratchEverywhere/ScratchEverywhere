#pragma once
#include "nonstd/expected.hpp"
#include <optional>
#ifdef ENABLE_SVG
#include "lunasvg.h"
#endif
#include <cstddef>
#include <memory.h>
#include <miniz.h>
#include <sprite.hpp>
#include <string>
#include <vector>

struct ImageSubrect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct ImageRenderParams {
    float x = 0.0f;
    float y = 0.0f;
    float scale = 1.0f;
    bool centered = true;
    float opacity = 1.0f;
    int brightness = 0;
    float rotation = 0;
    bool flip = false;
    ImageSubrect *subrect = nullptr;
};

enum ImageFormat {
    IMAGE_FORMAT_NONE,
    IMAGE_FORMAT_RGBA32, // most platforms use this
    IMAGE_FORMAT_PAL8,   // used by GL2D (NDS)
};

struct ImageData {
    int width, height;
    ImageFormat format;
    int pitch;
    void *pixels = nullptr;
    float scale = 1;
};

#ifdef ENABLE_SVG
struct SVGFont {
    std::string path;
    bool isLoaded = false;
};
#endif

class Image {
  private:
#ifdef ENABLE_SVG
    std::unique_ptr<lunasvg::Document> svgDocument = nullptr;
    static std::unordered_map<std::string, SVGFont> loadedFonts;
#endif

    bool loadFont(const std::string &family);
    inline nonstd::expected<std::vector<unsigned char>, std::string> readFileToBuffer(const std::string &filePath, bool fromScratchProject);
    inline nonstd::expected<unsigned char *, std::string> loadSVGFromMemory(const char *data, size_t size, int &width, int &height, float scale = 1);
    inline nonstd::expected<unsigned char *, std::string> loadRasterFromMemory(const unsigned char *data, size_t size, int &width, int &height, bool bitmapHalfQuality = false);
    inline unsigned char *resizeRaster(const unsigned char *srcPixels, int srcW, int srcH, int &outW, int &outH);

  protected:
    ImageData imgData;

    virtual nonstd::expected<void, std::string> refreshTexture() = 0;

    std::pair<unsigned int, unsigned int> maxTextureSize = {0, 0};

  public:
    const unsigned int maxFreeTimer = 540;
    unsigned int freeTimer = maxFreeTimer;

    /**
     * Set if an error occurs in the constructor.
     */
    std::optional<std::string> error;
    Image() {}
    Image(std::string filePath, bool fromScratchProject = true, bool bitmapHalfQuality = false, float scale = 1);
    Image(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality = false, float scale = 1);
    nonstd::expected<void, std::string> init(std::string filePath, bool fromScratchProject = true, bool bitmapHalfQuality = false, float scale = 1);
    nonstd::expected<void, std::string> init(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality = false, float scale = 1);
    virtual ~Image();

    virtual ImageData getPixels(ImageSubrect rect);
    inline ImageData getPixels() {
        return getPixels({.x = 0, .y = 0, .w = imgData.width, .h = imgData.height});
    }

    int getWidth();
    int getHeight();

    nonstd::expected<void, std::string> resizeSVG(float scale);

    virtual void *getNativeTexture() = 0;

    virtual void render(ImageRenderParams &params) = 0;
    virtual void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) = 0;
};

nonstd::expected<std::shared_ptr<Image>, std::string> createImageFromFile(std::string filePath, bool fromScratchProject = true, bool bitmapHalfQuality = false, float scale = 1);
nonstd::expected<std::shared_ptr<Image>, std::string> createImageFromZip(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality = false, float scale = 1);
