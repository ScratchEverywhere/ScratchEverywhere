#pragma once
#include <se_export.hpp>
#include "nonstd/expected.hpp"
#include <image.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class SE_EXPORT Image_GLCore : public Image {
  private:
    void setInitialTexture();

  public:
    unsigned int textureID = 0;

    Image_GLCore(std::string filePath, bool fromScratchProject = true, bool bitmapHalfQuality = false, float scale = 1);
    Image_GLCore(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality = false, float scale = 1);
    ~Image_GLCore() override;

    void render(ImageRenderParams &params) override;
    void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) override;

    void *getNativeTexture() override;

    nonstd::expected<void, std::string> refreshTexture() override;
};
