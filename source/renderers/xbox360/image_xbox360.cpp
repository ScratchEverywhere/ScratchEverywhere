#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <image_xbox360.hpp>
#include <log.hpp>
#include <renderer.hpp>
#include <stdexcept>
Image_Xbox360::Image_Xbox360(const ImageData &data) {
    if (!data.pixels || data.format != IMAGE_FORMAT_RGBA32 || data.scale != 1 || data.width <= 0 || data.height <= 0 || int64_t(data.pitch) < int64_t(data.width) * 4 || data.width > XSDK_TEXTURE_MAX_SIZE || data.height > XSDK_TEXTURE_MAX_SIZE)
        throw std::invalid_argument("Invalid Xbox 360 raw costume");
    if (uint64_t(data.pitch) * uint64_t(data.height) > SIZE_MAX) throw std::overflow_error("Costume byte size overflow");
    imgData = data;
    imgData.pixels = nullptr;
    imgData.pixels = malloc(size_t(data.pitch) * data.height);
    if (!imgData.pixels) throw std::bad_alloc();
    memset(imgData.pixels, 0, size_t(data.pitch) * data.height);
    for (int row = 0; row < data.height; ++row)
        memcpy(static_cast<uint8_t *>(imgData.pixels) + size_t(row) * data.pitch, static_cast<const uint8_t *>(data.pixels) + size_t(row) * data.pitch, size_t(data.width) * 4);
    auto result = refreshTexture();
    if (!result) throw std::runtime_error(result.error());
}
nonstd::expected<void, std::string> Image_Xbox360::refreshTexture() {
    try {
        auto replacement = std::make_unique<TexturedQuadXbox360>(imgData, Render::getWidth(), Render::getHeight());
        texture_ = std::move(replacement);
        return {};
    } catch (const std::exception &failure) {
        return nonstd::make_unexpected(std::string(failure.what()));
    }
}
void Image_Xbox360::render(ImageRenderParams &params) {
    if (params.opacity <= 0) return;
    if (!Render::getGlobalRenderer() || !Render::getGlobalRenderer()->hasFrameBegan)
        throw std::logic_error("Image render requires an open engine frame");
    texture_->render(params, getWidth(), getHeight(), Render::getWidth(), Render::getHeight());
}
void Image_Xbox360::renderNineslice(double x, double y, double width, double height, double border, bool centered) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height) || !std::isfinite(border))
        throw std::invalid_argument("Non-finite nineslice geometry");
    if (width <= 0 || height <= 0) return;
    if (!Render::getGlobalRenderer() || !Render::getGlobalRenderer()->hasFrameBegan)
        throw std::logic_error("Nineslice render requires an open engine frame");
    const double iw = getWidth(), ih = getHeight();
    if (iw <= 0 || ih <= 0) return;
    border = std::clamp(border, 0.0, std::min(iw, ih) / 2);
    const double bx = std::min(border, width / 2), by = std::min(border, height / 2);
    if (centered) {
        x -= width / 2;
        y -= height / 2;
    }
    const double dx[] = {0, bx, width - bx, width}, dy[] = {0, by, height - by, height};
    const double u[] = {0, border / iw, 1 - border / iw, 1}, v[] = {0, border / ih, 1 - border / ih, 1};
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col) {
            if (dx[col + 1] <= dx[col] || dy[row + 1] <= dy[row]) continue;
            ImageRenderParams p;
            p.x = x + dx[col];
            p.y = y + dy[row];
            p.centered = false;
            texture_->renderRegion(p, dx[col + 1] - dx[col], dy[row + 1] - dy[row], Render::getWidth(), Render::getHeight(), u[col], v[row], u[col + 1], v[row + 1]);
        }
}

// Use the engine's normal filesystem/archive resolution and bitmap/SVG decoding.
Image_Xbox360::Image_Xbox360(std::string path, bool fromProject, bool halfQuality, float scale) {
    try {
        // Shared PNG/SVG decoders produce RGBA bytes; the backend supplies the format tag.
        imgData.format = IMAGE_FORMAT_RGBA32;
        maxTextureSize = {XSDK_TEXTURE_MAX_SIZE, XSDK_TEXTURE_MAX_SIZE};
        auto decoded = Image::init(path, fromProject, halfQuality, scale);
        if (!decoded) {
            error = decoded.error();
            Log::logError("Xbox 360 image decode failed: " + path + ": " + *error);
            return;
        }
        auto uploaded = refreshTexture();
        if (!uploaded) error = uploaded.error();
        if (error) Log::logError("Xbox 360 image upload failed: " + path + ": " + *error);
    } catch (const std::exception &failure) {
        error = failure.what();
    }
}

Image_Xbox360::Image_Xbox360(std::string path, ZipArchive *zip, bool halfQuality, float scale) {
    try {
        // Shared PNG/SVG decoders produce RGBA bytes; the backend supplies the format tag.
        imgData.format = IMAGE_FORMAT_RGBA32;
        maxTextureSize = {XSDK_TEXTURE_MAX_SIZE, XSDK_TEXTURE_MAX_SIZE};
        auto decoded = Image::init(path, zip, halfQuality, scale);
        if (!decoded) {
            error = decoded.error();
            Log::logError("Xbox 360 image decode failed: " + path + ": " + *error);
            return;
        }
        auto uploaded = refreshTexture();
        if (!uploaded) error = uploaded.error();
        if (error) Log::logError("Xbox 360 image upload failed: " + path + ": " + *error);
    } catch (const std::exception &failure) {
        error = failure.what();
    }
}
