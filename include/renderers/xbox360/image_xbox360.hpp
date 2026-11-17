#pragma once
#include <image.hpp>
#include <memory>
#include <textured_quad_xbox360.hpp>

// Owns CPU pixels and one native texture, using the shared bitmap/SVG decoder.
class Image_Xbox360 final : public Image {
    std::unique_ptr<TexturedQuadXbox360> texture_;
    nonstd::expected<void, std::string> refreshTexture() override;

  public:
    explicit Image_Xbox360(const ImageData &data);
    Image_Xbox360(std::string path, bool fromProject, bool halfQuality, float scale);
    Image_Xbox360(std::string path, ZipArchive *zip, bool halfQuality, float scale);
    ~Image_Xbox360() override = default;
    bool ensureColorData() override { return imgData.pixels != nullptr; }
    void *getNativeTexture() override { return texture_.get(); }
    void render(ImageRenderParams &params) override;
    void renderNineslice(double, double, double, double, double, bool) override;
};
