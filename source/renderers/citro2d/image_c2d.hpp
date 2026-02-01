#pragma once
#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <image.hpp>
#include <string>

class Image_C2D : public Image {
  private:
    void setInitialTexture();
    const uint32_t rgbaToAgbr(uint32_t px);
    void renderSubrect(C2D_Image img, uint16_t srcX, uint16_t srcY, uint16_t srcW, uint16_t srcH, float destX, float destY, float destW, float destH, C2D_ImageTint *tint);

  public:
    C2D_Image texture;

    Image_C2D(std::string filePath, bool fromScratchProject = true, float bitmapQuality = 1.0f);

    Image_C2D(std::string filePath, mz_zip_archive *zip, float bitmapQuality = 1.0f);

    ~Image_C2D() override;

    ImageData getPixels(ImageSubrect rect) override;

    void render(ImageRenderParams &params) override;
    void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) override;
};