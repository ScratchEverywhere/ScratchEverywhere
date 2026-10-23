#pragma once
#include "nonstd/expected.hpp"
#define WIN32_MEAN_AND_LEAN
#include <d3d9.h>
#include <image.hpp>
#include <se_export.hpp>
#include <string>
#include <windows.h>

class SE_EXPORT Image_D3D9 : public Image {
  private:
    void setInitialTexture();

    IDirect3DTexture9 *pTex;

  public:
    Image_D3D9(std::string filePath, bool fromScratchProject = true, bool bitmapHalfQuality = false, float scale = 1);

    Image_D3D9(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality = false, float scale = 1);

    ~Image_D3D9() override;

    void render(ImageRenderParams &params) override;
    void renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered = false) override;

    void *getNativeTexture() override;

    nonstd::expected<void, std::string> refreshTexture() override;
};
