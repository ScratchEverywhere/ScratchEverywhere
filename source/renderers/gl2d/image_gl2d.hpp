#pragma once
#include <gl2d.h>
#include <image.hpp>
#include <nds.h>
#include <unordered_map>

class Image_GL2D : public Image {
  private:
    void setInitialTexture();
    void RGBAToPAL8();
    void *resizeRGBAImage(uint16_t newWidth, uint16_t newHeight);

    unsigned char *textureData;
    unsigned short *paletteData;
    int paletteSize;
    int paletteID;
    int resizedWidth = 0;
    int resizedHeight = 0;

  public:
    int textureID;
    glImage texture;

    Image_GL2D(std::string filePath, bool fromScratchProject = true);

    Image_GL2D(std::string filePath, mz_zip_archive *zip);

    ~Image_GL2D() override;

    ImageData getPixels(ImageSubrect rect) override;

    void render(ImageRenderParams &params) override;
    void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) override;
};
