#pragma once
#include <image.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class Image_GL : public Image {
  private:
    void setInitialTexture();

  public:
    unsigned int textureID;
    Image_GL(std::string filePath, bool fromScratchProject = true, bool bitmapHalfQuality = false);

    Image_GL(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality = false);

    ~Image_GL() override;

    void render(ImageRenderParams &params) override;
    void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) override;

    // this just returns the TextureID as a void*
    void getNativeTexture() override;
};