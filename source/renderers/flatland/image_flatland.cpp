#include "image_flatland.hpp"
#include "flatland_backend.hpp"
#include "nonstd/expected.hpp"
#include <log.hpp>

namespace {
constexpr int maxTextureSide = 2048;
} // namespace

nonstd::expected<void, std::string> Image_Flatland::registerTexture() {
    FlatlandBackend &backend = FlatlandBackend::get();

    textureId = backend.findTexture(sourceName);
    if (textureId < 0 && !backend.isSealed()) {
        if (imgData.width > maxTextureSide || imgData.height > maxTextureSide) {
            Log::logWarning("Costume is too large for Flatland (" + std::to_string(imgData.width) + "x" + std::to_string(imgData.height) + "): " + sourceName);
        } else {
            textureId = backend.registerTexture(sourceName, FlatlandBackend::encodePng(imgData.pixels, imgData.width, imgData.height, imgData.pitch));
        }
    }
    if (textureId < 0) Log::logWarning("Costume is not in the Flatland resource pack: " + sourceName);

    discardColorData();
    return {};
}

nonstd::expected<void, std::string> Image_Flatland::refreshTexture() {
    discardColorData();
    return {};
}

Image_Flatland::Image_Flatland(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality, float scale) : sourceName(filePath) {
    const auto initResult = init(filePath, zip, bitmapHalfQuality, scale);
    if (!initResult.has_value()) {
        error = initResult.error();
        return;
    }

    const auto result = registerTexture();
    if (!result.has_value()) error = result.error();
}

Image_Flatland::Image_Flatland(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) : sourceName(filePath) {
    const auto initResult = init(filePath, fromScratchProject, bitmapHalfQuality, scale);
    if (!initResult.has_value()) {
        error = initResult.error();
        return;
    }

    const auto result = registerTexture();
    if (!result.has_value()) error = result.error();
}

Image_Flatland::~Image_Flatland() = default;

void Image_Flatland::render(ImageRenderParams &params) {}

void Image_Flatland::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {}

void *Image_Flatland::getNativeTexture() {
    return nullptr;
}
