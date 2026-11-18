#pragma once
#include <algorithm>
#include <cmath>
#include <memory>
#include <renderer.hpp>
#include <stdexcept>
#include <text.hpp>
#include <textured_quad_xbox360.hpp>

// Use the shared TrueType layout, with a cached, antialiased native label texture.
// Fractional display scaling reuses the texture; bucket/pack/text/color changes rebuild it.
class TextObjectXbox360 final : public TextObjectBase {
    std::unique_ptr<TexturedQuadXbox360> texture_;
    std::string cachedText_;
    int cachedBucket_ = -1;
    unsigned cachedPack_ = 0;
    uint32_t cachedColor_ = 0;
    float layoutScale_ = -1;
    int originX_ = 0, originY_ = 0, width_ = 0, height_ = 0;

    void syncLayout() {
        if (scale != layoutScale_) {
            relayout();
            layoutScale_ = scale;
        }
    }
    void uploadAtlas(FontGeneration &gen) override {
        const float lineHeight = gen.ascent - gen.descent + gen.lineGap;
        float minX = 0, minY = 0, maxX = layoutWidth / glyphScale, maxY = layoutHeight / glyphScale;
        for (size_t line = 0; line < layoutLines.size(); ++line) {
            const float baseline = line * lineHeight + gen.ascent;
            for (const auto &q : layoutLines[line]) {
                minX = std::min(minX, q.x0);
                minY = std::min(minY, baseline + q.y0);
                maxX = std::max(maxX, q.x1);
                maxY = std::max(maxY, baseline + q.y1);
            }
        }
        originX_ = int(std::floor(minX)) - 1;
        originY_ = int(std::floor(minY)) - 1;
        const int width = int(std::ceil(maxX)) - originX_ + 1;
        const int height = int(std::ceil(maxY)) - originY_ + 1;
        if (width <= 0 || height <= 0 || width > XSDK_TEXTURE_MAX_SIZE || height > XSDK_TEXTURE_MAX_SIZE)
            throw std::runtime_error("Xbox 360 text exceeds native texture bounds");
        const uint32_t rgbaColor = uint32_t(color);
        std::vector<uint8_t> rgba(size_t(width) * height * 4);
        for (size_t i = 0; i < rgba.size(); i += 4) {
            rgba[i] = rgbaColor >> 24;
            rgba[i + 1] = rgbaColor >> 16;
            rgba[i + 2] = rgbaColor >> 8;
        }
        auto sample = [&](int x, int y) {
            x = std::clamp(x, 0, gen.atlasWidth - 1);
            y = std::clamp(y, 0, gen.atlasHeight - 1);
            return float(gen.pixels[size_t(y) * gen.atlasWidth + x]);
        };
        for (size_t line = 0; line < layoutLines.size(); ++line) {
            const float baseline = line * lineHeight + gen.ascent;
            for (const auto &q : layoutLines[line]) {
                const float x0 = q.x0 - originX_, y0 = baseline + q.y0 - originY_;
                const float x1 = q.x1 - originX_, y1 = baseline + q.y1 - originY_;
                if (x1 <= x0 || y1 <= y0) continue;
                for (int y = std::max(0, int(std::floor(y0))); y < std::min(height, int(std::ceil(y1))); ++y) {
                    for (int x = std::max(0, int(std::floor(x0))); x < std::min(width, int(std::ceil(x1))); ++x) {
                        const float u = q.s0 + (x + 0.5f - x0) / (x1 - x0) * (q.s1 - q.s0);
                        const float v = q.t0 + (y + 0.5f - y0) / (y1 - y0) * (q.t1 - q.t0);
                        const float tx = u * gen.atlasWidth - 0.5f, ty = v * gen.atlasHeight - 0.5f;
                        const int sx = int(std::floor(tx)), sy = int(std::floor(ty));
                        const float fx = tx - sx, fy = ty - sy;
                        const float coverage = (sample(sx, sy) * (1 - fx) + sample(sx + 1, sy) * fx) * (1 - fy) +
                                               (sample(sx, sy + 1) * (1 - fx) + sample(sx + 1, sy + 1) * fx) * fy;
                        const unsigned alpha = unsigned(std::clamp(std::round(coverage * (rgbaColor & 255) / 255), 0.0f, 255.0f));
                        auto &dst = rgba[(size_t(y) * width + x) * 4 + 3];
                        dst = uint8_t(alpha + unsigned(dst) * (255 - alpha) / 255);
                    }
                }
            }
        }
        ImageData data;
        data.width = width;
        data.height = height;
        data.pitch = width * 4;
        data.format = IMAGE_FORMAT_RGBA32;
        data.pixels = rgba.data();
        auto replacement = std::make_unique<TexturedQuadXbox360>(data, Render::getWidth(), Render::getHeight(), true);
        texture_ = std::move(replacement);
        width_ = width;
        height_ = height;
        cachedText_ = text;
        cachedBucket_ = fontBucket;
        cachedPack_ = gen.packVersion;
        cachedColor_ = rgbaColor;
    }

  public:
    TextObjectXbox360(std::string value, double x, double y, std::string font) : TextObjectBase(value, x, y, font, 30.0f) {}
    std::vector<float> getSize() override {
        syncLayout();
        return TextObjectBase::getSize();
    }
    void render(int xPos, int yPos) override {
        if (!std::isfinite(scale) || scale <= 0 || text.empty()) return;
        if (!Render::getGlobalRenderer() || !Render::getGlobalRenderer()->hasFrameBegan || !fontAtlas || !fontAtlas->isValid()) return;
        syncLayout();
        FontGeneration &gen = touchGeneration();
        if (!texture_ || cachedText_ != text || cachedBucket_ != fontBucket || cachedPack_ != gen.packVersion || cachedColor_ != uint32_t(color)) uploadAtlas(gen);
        ImageRenderParams p;
        p.centered = false;
        p.scale = glyphScale;
        p.x = xPos - (centerAligned ? layoutWidth / 2 : 0) + originX_ * glyphScale;
        p.y = yPos - (centerAligned ? layoutHeight / 2 : 0) + originY_ * glyphScale;
        texture_->render(p, width_, height_, Render::getWidth(), Render::getHeight());
    }
};
