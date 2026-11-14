#include "image.hpp"
#include "nonstd/expected.hpp"
#include "os.hpp"
#include "renderer.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <log.hpp>
#include <sb2_bitmap_layers.hpp>
#ifdef __XBOX__
#include <runtime.hpp>
#endif
#ifdef __VFLASH__
#include <miniz.h>
#endif
#include <stdexcept>
#include <string_view>
#include <unzip.hpp>

static bool hasSVGExtension(const std::string &path) {
    if (path.size() < 4) return false;
    const size_t offset = path.size() - 4;
    return path[offset] == '.' &&
           std::tolower(static_cast<unsigned char>(path[offset + 1])) == 's' &&
           std::tolower(static_cast<unsigned char>(path[offset + 2])) == 'v' &&
           std::tolower(static_cast<unsigned char>(path[offset + 3])) == 'g';
}

#ifdef ENABLE_BITMAP
#if defined(__WIIU__) || defined(__XBOX__)
#define STBI_NO_THREAD_LOCALS
#endif
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#endif

#ifdef ENABLE_SVG
#include <lunasvg.h>
#include <scratch2_svg.hpp>
#include <scratch2_svg_bounds.hpp>
#endif

#if defined(RENDERER_SDL1)
#include <image_sdl1.hpp>
#endif
#if defined(RENDERER_SDL2)
#include <image_sdl2.hpp>
#endif
#if defined(RENDERER_VFLASH)
#include <image_vflash.hpp>
#endif
#if defined(RENDERER_XBOX)
#include <image_xbox.hpp>
#endif
#if defined(RENDERER_NV2A)
#include <image_nv2a.hpp>
#endif
#if defined(RENDERER_SDL3)
#include <image_sdl3.hpp>
#endif
#if defined(RENDERER_OPENGL)
#include <image_gl.hpp>
#endif
#if defined(RENDERER_CITRO2D)
#include <image_c2d.hpp>
#endif
#if defined(RENDERER_GL2D)
#include <image_gl2d.hpp>
#endif
#if defined(RENDERER_OPENGL_CORE)
#include <image_gl_core.hpp>
#endif
#if defined(RENDERER_GX2)
#include <image_gx2.hpp>
#endif
#if defined(RENDERER_GDI)
#include <image_gdi.hpp>
#endif
#if defined(RENDERER_HEADLESS)
#include <image_headless.hpp>
#endif
#if defined(RENDERER_D3D9)
#include <image_d3d9.hpp>
#endif

#ifdef USE_BUNDLE
#include <bundle_archives.hpp>
#endif

std::unordered_map<std::string, std::weak_ptr<Image>> images;

#ifdef ENABLE_SVG
std::unordered_map<std::string, SVGFont> Image::loadedFonts = {
    {"", {"gfx/ingame/fonts/NotoSerif-Regular", false}},
    {"Sans Serif", {"gfx/ingame/fonts/NotoSans-Medium", false}},
    {"Scratch", {"gfx/ingame/fonts/ScratchSavers", false}},
    {"Handwriting", {"gfx/ingame/fonts/Handlee-Regular", false}},
    {"Marker", {"gfx/ingame/fonts/Knewave-Regular", false}},
    {"Curly", {"gfx/ingame/fonts/Griffy-Regular", false}},
    {"Pixel", {"gfx/ingame/fonts/Grand9KPixel", false}}};
#endif

constexpr unsigned int maxScale = 5; // TODO: Make project setting, set to 0 to remove scaling limit.

bool Image::loadFont(const std::string &family) {
#ifdef ENABLE_SVG
    auto it = loadedFonts.find(family);
    if (it == loadedFonts.end()) {
        return false;
    }
    if (it->second.isLoaded) return true;

    if (it->second.fromProject) {
        void *fontData = nullptr;
        size_t fontSize = 0;
        if (Scratch::projectType == ProjectType::UNZIPPED) {
            auto buffer = readFileToBuffer(Unzip::UnpackedInSD ? Unzip::filePath + it->second.path : OS::getRomFSLocation() + "project/" + it->second.path, true);
            if (!buffer.has_value() || buffer->size() < 2) return false;
            fontSize = buffer->size() - 1;
            fontData = malloc(fontSize);
            if (fontData) memcpy(fontData, buffer->data(), fontSize);
        } else if (Scratch::sb3InRam && Unzip::zipArchive) {
            void *raw = Unzip::zipArchive->extractToHeap(it->second.path, &fontSize);
            if (raw && fontSize > 0) {
                fontData = malloc(fontSize);
                if (fontData) memcpy(fontData, raw, fontSize);
            }
            if (raw) Unzip::zipArchive->freeHeap(raw);
        } else {
            fontData = Unzip::getFileInSB3(it->second.path, &fontSize);
        }
        if (!fontData || fontSize == 0) {
            free(fontData);
            return false;
        }
        if (!lunasvg_add_font_face_from_data(family.c_str(), false, false, fontData, fontSize, [](void *p) { free(p); }, fontData)) return false;
        it->second.isLoaded = true;
        return true;
    }

    const std::string &path = Render::currentRenderer == Renderers::Headless ? std::string("gfx/ingame/fonts/Grand9KPixel") : it->second.path; // font doesn't matter for headless and Grand9KPixel is smallest on memory

#ifdef USE_BUNDLE
    const auto assets = Bundle::assets();
    auto entry = assets.find(OS::getRomFSLocation() + path + ".ttf");
    if (!entry) return false;
    size_t fontSize = 0;
    void *fontData = bundle_load(&entry->raw(), &fontSize);
    if (!fontData) return false;
    if (!lunasvg_add_font_face_from_data(family.c_str(), false, false, fontData, fontSize, [](void *p) { free(p); }, fontData)) return false;
#else
    if (!lunasvg_add_font_face_from_file(family.c_str(), false, false, (OS::getRomFSLocation() + path + ".ttf").c_str())) return false;
#endif
    it->second.isLoaded = true;
    return true;
#endif
    return false;
}

void Image::registerProjectFont(const std::string &family, const std::string &fileName) {
#ifdef ENABLE_SVG
    loadedFonts[family] = SVGFont{fileName, false, true};
#endif
}

void Image::clearProjectFonts() {
#ifdef ENABLE_SVG
    for (auto it = loadedFonts.begin(); it != loadedFonts.end();) {
        if (it->second.fromProject) it = loadedFonts.erase(it);
        else ++it;
    }
#endif
}

nonstd::expected<std::shared_ptr<Image>, std::string> createImageFromFile(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) {
    auto it = images.find(filePath);
    if (it != images.end()) {
        if (auto img = it->second.lock()) {
            return img;
        } else {
            images.erase(it);
        }
    }

    Image *rawImg = nullptr;

    switch (Render::currentRenderer) {
#if defined(RENDERER_SDL1)
    case Renderers::SDL1:
        rawImg = new Image_SDL1(filePath, fromScratchProject, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_SDL2)
    case Renderers::SDL2:
        rawImg = new Image_SDL2(filePath, fromScratchProject, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_VFLASH)
    case Renderers::VFlash:
        rawImg = new Image_VFlash(filePath, fromScratchProject, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_XBOX)
    case Renderers::Xbox:
        rawImg = new Image_Xbox(filePath, fromScratchProject, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_NV2A)
    case Renderers::NV2A:
        rawImg = new Image_NV2A(filePath, fromScratchProject, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_SDL3)
    case Renderers::SDL3:
        rawImg = new Image_SDL3(filePath, fromScratchProject, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_CITRO2D)
    case Renderers::Citro2D:
        rawImg = new Image_C2D(filePath, fromScratchProject, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_OPENGL)
    case Renderers::OpenGL:
        rawImg = new Image_GL(filePath, fromScratchProject, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_GL2D)
    case Renderers::GL2D:
        rawImg = new Image_GL2D(filePath, fromScratchProject, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_OPENGL_CORE)
    case Renderers::OpenGLCore:
        rawImg = new Image_GLCore(filePath, fromScratchProject, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_GX2)
    case Renderers::GX2:
        rawImg = new Image_GX2(filePath, fromScratchProject, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_GDI)
    case Renderers::GDI:
        rawImg = new Image_GDI(filePath, fromScratchProject, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_D3D9)
    case Renderers::D3D9:
        rawImg = new Image_D3D9(filePath, fromScratchProject, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_HEADLESS)
    case Renderers::Headless:
        rawImg = new Image_Headless(filePath, fromScratchProject, bitmapHalfQuality, scale);
        break;
#endif
    default: {
        Log::logCritical("You forgot to add this renderer to createImageFromFile :grin: :grin:", true);
        break;
    }
    }
    if (rawImg == nullptr) {
        return nonstd::make_unexpected("Failed to create image.");
    } else if (rawImg->error.has_value()) {
        const std::string errMessage = rawImg->error.value();
        delete rawImg;
        return nonstd::make_unexpected(errMessage);
    }

    auto img = std::shared_ptr<Image>(rawImg, [filePath](Image *p) {
        images.erase(filePath);
        delete p;
    });

    images[filePath] = img;
    return img;
}

nonstd::expected<std::shared_ptr<Image>, std::string> createImageFromZip(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality, float scale) {
    auto it = images.find(filePath);
    if (it != images.end()) {
        if (auto img = it->second.lock()) {
            return img;
        } else {
            images.erase(it);
        }
    }

    Image *rawImg = nullptr;

    switch (Render::currentRenderer) {
#if defined(RENDERER_SDL1)
    case Renderers::SDL1:
        rawImg = new Image_SDL1(filePath, zip, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_SDL2)
    case Renderers::SDL2:
        rawImg = new Image_SDL2(filePath, zip, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_VFLASH)
    case Renderers::VFlash:
        rawImg = new Image_VFlash(filePath, zip, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_XBOX)
    case Renderers::Xbox:
        rawImg = new Image_Xbox(filePath, zip, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_NV2A)
    case Renderers::NV2A:
        rawImg = new Image_NV2A(filePath, zip, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_SDL3)
    case Renderers::SDL3:
        rawImg = new Image_SDL3(filePath, zip, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_CITRO2D)
    case Renderers::Citro2D:
        rawImg = new Image_C2D(filePath, zip, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_OPENGL)
    case Renderers::OpenGL:
        rawImg = new Image_GL(filePath, zip, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_GL2D)
    case Renderers::GL2D:
        rawImg = new Image_GL2D(filePath, zip, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_OPENGL_CORE)
    case Renderers::OpenGLCore:
        rawImg = new Image_GLCore(filePath, zip, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_GX2)
    case Renderers::GX2:
        rawImg = new Image_GX2(filePath, zip, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_GDI)
    case Renderers::GDI:
        rawImg = new Image_GDI(filePath, zip, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_D3D9)
    case Renderers::D3D9:
        rawImg = new Image_D3D9(filePath, zip, bitmapHalfQuality, scale);
        break;
#endif
#if defined(RENDERER_HEADLESS)
    case Renderers::Headless:
        rawImg = new Image_Headless(filePath, zip, bitmapHalfQuality, scale);
        break;
#endif
    default: {
        Log::logCritical("You forgot to add this renderer to createImageFromZip :grin: :grin:", true);
        break;
    }
    }

    if (rawImg == nullptr) {
        return nonstd::make_unexpected("Failed to create image.");
    } else if (rawImg->error.has_value()) {
        const std::string errMessage = rawImg->error.value();
        delete rawImg;
        return nonstd::make_unexpected(errMessage);
    }

    auto img = std::shared_ptr<Image>(rawImg, [filePath](Image *p) {
        images.erase(filePath);
        delete p;
    });

    images[filePath] = img;
    return img;
}

nonstd::expected<std::vector<unsigned char>, std::string> Image::readFileToBuffer(const std::string &filePath, bool fromScratchProject) {
#ifdef USE_BUNDLE
    if (!Unzip::UnpackedInSD || !fromScratchProject) {
        auto archive = fromScratchProject ? Bundle::project() : Bundle::assets();
        auto entry = archive.find(filePath);
        if (!entry) return nonstd::make_unexpected("File not found: " + filePath);
        std::vector<unsigned char> buffer(entry->size() + 1);
        if (bundle_read_all(&entry->raw(), buffer.data(), entry->size()) < 0) return nonstd::make_unexpected("Failed to decompress file: " + filePath);
        buffer[entry->size()] = '\0';
        return buffer;
    }
#endif

    std::string path = filePath;

    FILE *file = fopen(path.c_str(), "rb");
    if (!file) return nonstd::make_unexpected("Failed to open file: " + path);

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0) {
        fclose(file);
        return nonstd::make_unexpected("Empty file: " + path);
    }

    std::vector<unsigned char> buffer(size + 1);
    if (fread(buffer.data(), 1, size, file) != (size_t)size) {
        fclose(file);
        return nonstd::make_unexpected("Failed to read file: " + path);
    }
    buffer[size] = '\0';
    fclose(file);
    return buffer;
}

#ifdef ENABLE_SVG
inline void reorderPixels(unsigned char *src, unsigned char *dst, const size_t pixelsSize) {
    for (size_t i = 0; i < pixelsSize; i += 4) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        unsigned char a = src[i + 0];
        unsigned char r = src[i + 1];
        unsigned char g = src[i + 2];
        unsigned char b = src[i + 3];
#else
        unsigned char b = src[i + 0];
        unsigned char g = src[i + 1];
        unsigned char r = src[i + 2];
        unsigned char a = src[i + 3];
#endif
        // LunaSVG multiplies the colors when there's alpha present, so we gotta un-mulitply it
        if (a > 0 && a < 255) {
            r = (unsigned char)((r * 255) / a);
            g = (unsigned char)((g * 255) / a);
            b = (unsigned char)((b * 255) / a);
        }

        dst[i + 0] = r;
        dst[i + 1] = g;
        dst[i + 2] = b;
        dst[i + 3] = a;
    }
}
#endif

void Image::releaseSVGDocumentIfFinal(float requestedScale, float renderedScale) {
#ifdef ENABLE_SVG
    if (renderedScale < requestedScale || (maxScale != 0 && renderedScale >= maxScale)) svgDocument.reset();
#endif
}

#ifdef ENABLE_SVG
static std::pair<float, float> svgViewBoxOrigin(lunasvg::Document &document) {
    const std::string viewBox = document.documentElement().getAttribute("viewBox");
    float x = 0, y = 0, w = 0, h = 0;
    if (std::sscanf(viewBox.c_str(), "%f%*[ ,]%f%*[ ,]%f%*[ ,]%f", &x, &y, &w, &h) != 4) return {0, 0};
    if (!std::isfinite(x) || !std::isfinite(y)) return {0, 0};
    return {x, y};
}
#endif

nonstd::expected<unsigned char *, std::string> Image::loadSVGFromMemory(const char *data, size_t size, int &width, int &height, float scale, bool scratch2) {
#ifdef ENABLE_SVG
    if constexpr (maxScale != 0)
        if (scale > maxScale) scale = maxScale;

    const std::string_view svgView(data, size);

    // always load default font if there is text present
    if (svgView.find("<text") != std::string_view::npos || svgView.find("<tspan") != std::string_view::npos) {
        loadFont("");
    }

    for (const auto &[family, entry] : loadedFonts) {
        if (family.empty() || entry.isLoaded) continue;

        if (svgView.find(family) != std::string_view::npos) {
            loadFont(family);
        }
    }

#if defined(__XBOX__)
    std::string svgData(data, size);
    size_t endPos = svgData.rfind("</svg>");
    if (endPos == std::string::npos) {
        endPos = svgData.rfind("</SVG>");
    }
    if (endPos != std::string::npos) {
        svgData.resize(endPos + 6);
    }

    if (scratch2) svgData = Scratch2Svg::normalizeText(svgData);
    svgDocument = lunasvg::Document::loadFromData(svgData.c_str());
    if (!svgDocument) {
        return nonstd::make_unexpected("LunaSVG failed to parse SVG");
    }
    svgViewOffset = scratch2 ? Scratch2Svg::normalizeMeasurements(*svgDocument) : svgViewBoxOrigin(*svgDocument);

    float docW = svgDocument->width();
    float docH = svgDocument->height();
    logicalWidth = docW;
    logicalSizeKnown = true;
    logicalHeight = docH;

    if (docW <= 0.0f || docH <= 0.0f) {
        width = 1;
        height = 1;
        imgData.scale = 1.0f;
        imgData.pitch = 4;
        unsigned char *dst = (unsigned char *)calloc(1, 4);
        if (!dst) return nonstd::make_unexpected("Failed to allocate SVG pixels buffer");
        return dst;
    }

    const float targetWidth = docW * scale;
    const float targetHeight = docH * scale;
#else
    const std::string svgData = scratch2 ? Scratch2Svg::normalizeText(svgView) : std::string(svgView);
    svgDocument = lunasvg::Document::loadFromData(svgData.c_str());
    if (!svgDocument) return nonstd::make_unexpected("LunaSVG failed to parse SVG");
    svgViewOffset = scratch2 ? Scratch2Svg::normalizeMeasurements(*svgDocument) : svgViewBoxOrigin(*svgDocument);

    float docW = svgDocument->width();
    float docH = svgDocument->height();
    logicalWidth = docW;
    logicalSizeKnown = true;
    logicalHeight = docH;

    if (docW <= 0.0f || docH <= 0.0f) {
        width = 1;
        height = 1;
        imgData.scale = 1.0f;
        imgData.pitch = 4;
        unsigned char *dst = (unsigned char *)calloc(1, 4);
        if (!dst) return nonstd::make_unexpected("Failed to allocate SVG pixels buffer");
        return dst;
    }

    const float targetWidth = svgDocument->width() * scale;
    const float targetHeight = svgDocument->height() * scale;
#endif

    const auto [maxWidth, maxHeight] = maxTextureSize;
    float finalScale = scale;
    if (maxWidth > 0 && maxHeight > 0) {
        if (targetWidth > maxWidth || targetHeight > maxHeight) {
            const float ratioWidth = (float)maxWidth / targetWidth;
            const float ratioHeight = (float)maxHeight / targetHeight;
            finalScale *= std::min(ratioWidth, ratioHeight);
        }
    }

#if defined(__XBOX__)
    width = std::max(1, (int)(docW * finalScale));
    height = std::max(1, (int)(docH * finalScale));
    imgData.scale = finalScale;
    svgRasterScale = scale;
    svgRasterScale = scale;

    auto bitmap = svgDocument->renderToBitmap(width, height);
    if (!bitmap.valid()) {
        svgDocument.reset();
        return nonstd::make_unexpected("LunaSVG failed to render SVG to bitmap");
    }
#else
    width = std::max(1, (int)(svgDocument->width() * finalScale));
    height = std::max(1, (int)(svgDocument->height() * finalScale));
    imgData.scale = finalScale;
    svgRasterScale = scale;
    svgRasterScale = scale;

    if (Render::currentRenderer == Renderers::Headless) {
        imgData.pitch = width * 4;
        svgDocument.reset();
        return (unsigned char *)calloc(1, 4);
    }

    auto bitmap = svgDocument->renderToBitmap(width, height);
    if (!bitmap.valid()) return nonstd::make_unexpected("LunaSVG failed to render SVG to bitmap");
#endif

    unsigned char *src = bitmap.data();
    const size_t pixelsSize = width * height * 4;
    unsigned char *dst = (unsigned char *)malloc(pixelsSize);
#ifdef __XBOX__
    if (!dst) {
        Scratch::freeUnusedCostumeImages(this);
        dst = (unsigned char *)malloc(pixelsSize);
    }
#endif
    if (!dst) {
#ifdef __XBOX__
        svgDocument.reset();
#endif
        return nonstd::make_unexpected("Failed to allocate SVG pixels buffer");
    }

    reorderPixels(src, dst, pixelsSize);
    releaseSVGDocumentIfFinal(scale, finalScale);

    imgData.pitch = width * 4;

#if defined(__XBOX__)
    svgDocument.reset();
#endif

    return dst;
#endif
    width = 0;
    height = 0;
    scale = 1.0f;
    return nullptr;
}

nonstd::expected<void, std::string> Image::resizeSVG(float scale) {
#ifdef ENABLE_SVG
    if constexpr (maxScale == 1) return {};

    if constexpr (maxScale != 0)
        if (scale > maxScale) scale = maxScale;

    if (!svgDocument || scale <= svgRasterScale) return {};
    if (svgDocument->width() <= 0.0f || svgDocument->height() <= 0.0f) return {};

    // TODO: De-duplicate code.

    const float targetWidth = svgDocument->width() * scale;
    const float targetHeight = svgDocument->height() * scale;

    const auto [maxWidth, maxHeight] = maxTextureSize;
    float finalScale = scale;
    if (maxWidth > 0 && maxHeight > 0) {
        if (targetWidth > maxWidth || targetHeight > maxHeight) {
            const float ratioWidth = (float)maxWidth / targetWidth;
            const float ratioHeight = (float)maxHeight / targetHeight;
            finalScale *= std::min(ratioWidth, ratioHeight);
        }
    }

    const int width = std::max(1, (int)(svgDocument->width() * finalScale));
    const int height = std::max(1, (int)(svgDocument->height() * finalScale));

    auto bitmap = svgDocument->renderToBitmap(width, height);
    if (!bitmap.valid()) return nonstd::make_unexpected("LunaSVG failed to render SVG to bitmap");

    unsigned char *src = bitmap.data();
    const size_t pixelsSize = width * height * 4;
    unsigned char *dst = (unsigned char *)malloc(pixelsSize);
#ifdef __XBOX__
    if (!dst) {
        Scratch::freeUnusedCostumeImages(this);
        dst = (unsigned char *)malloc(pixelsSize);
    }
#endif
    if (!dst) return nonstd::make_unexpected("Failed to allocate SVG pixels buffer");

    reorderPixels(src, dst, pixelsSize);
    releaseSVGDocumentIfFinal(scale, finalScale);

    if (imgData.pixels != nullptr) free(imgData.pixels);

    imgData.width = width;
    imgData.height = height;
    imgData.scale = finalScale;
    svgRasterScale = scale;
    imgData.pitch = width * 4;
    imgData.pixels = dst;

    return refreshTexture();
#endif

    return {};
}

unsigned char *Image::resizeRaster(const unsigned char *srcPixels, int srcW, int srcH, int &outW, int &outH, const Image *keep) {
    if (srcW <= 0 || srcH <= 0 || !srcPixels) return nullptr;

    outW = srcW >> 1;
    outH = srcH >> 1;

    if (outW <= 0) outW = 1;
    if (outH <= 0) outH = 1;

    unsigned char *dst = (unsigned char *)malloc(static_cast<size_t>(outW) * outH * 4);
#ifdef __XBOX__
    if (!dst) {
        Scratch::freeUnusedCostumeImages(keep);
        dst = (unsigned char *)malloc(static_cast<size_t>(outW) * outH * 4);
    }
#endif

    const int srcStride = srcW * 4;
    const int dstStride = outW * 4;

    if (!dst) return nullptr;

    for (int y = 0; y < outH; ++y) {
        const unsigned char *srcRow = srcPixels + (y << 1) * srcStride;
        unsigned char *dstRow = dst + y * dstStride;

        for (int x = 0; x < outW; ++x) {
            const unsigned char *p = srcRow + (x << 1) * 4;

            *(uint32_t *)dstRow = *(const uint32_t *)p;

            dstRow += 4;
        }
    }

    return dst;
}

#ifdef __VFLASH__
static uint8_t paethPredictor(uint8_t a, uint8_t b, uint8_t c) {
    int p = static_cast<int>(a) + static_cast<int>(b) - static_cast<int>(c);
    int pa = std::abs(p - static_cast<int>(a));
    int pb = std::abs(p - static_cast<int>(b));
    int pc = std::abs(p - static_cast<int>(c));
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

static unsigned char *decodePngDownscaledVFlash(const unsigned char *data, size_t size, int &outW, int &outH, float &imgScale) {
    if (!data || size < 8) return nullptr;
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (std::memcmp(data, sig, 8) != 0) return nullptr;

    auto readU32BE = [](const uint8_t *p) -> uint32_t {
        return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
               (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
    };

    int origW = 0, origH = 0, bpp = 0;
    uint8_t colorType = 0;
    size_t rowBytes = 0;
    std::vector<uint8_t> prevRow, curRow;
    unsigned char *outPixels = nullptr;
    uint8_t palette[256][3]{};
    uint8_t trnsAlpha[256];
    std::memset(trnsAlpha, 255, sizeof(trnsAlpha));

    mz_stream zs{};
    bool zsInit = false;
    size_t curRowPos = 0;
    int curY = 0;
    int nextDy = 0;

    auto cleanupFail = [&]() -> unsigned char * {
        if (zsInit) mz_inflateEnd(&zs);
        std::free(outPixels);
        return nullptr;
    };

    size_t pos = 8;
    while (pos + 12 <= size) {
        uint32_t len = readU32BE(data + pos);
        if (pos + 12 + static_cast<size_t>(len) > size) return cleanupFail();
        const uint8_t *type = data + pos + 4;
        const uint8_t *cdata = data + pos + 8;

        if (std::memcmp(type, "IHDR", 4) == 0 && len >= 13) {
            origW = static_cast<int>(readU32BE(cdata));
            origH = static_cast<int>(readU32BE(cdata + 4));
            uint8_t bitDepth = cdata[8];
            colorType = cdata[9];
            uint8_t comp = cdata[10], filt = cdata[11], interlace = cdata[12];
            if (origW <= 0 || origH <= 0 || bitDepth != 8 || comp != 0 || filt != 0 || interlace != 0) return cleanupFail();
            if (colorType == 0 || colorType == 3) bpp = 1;
            else if (colorType == 2) bpp = 3;
            else if (colorType == 4) bpp = 2;
            else if (colorType == 6) bpp = 4;
            else return cleanupFail();

            const int maxW = 160, maxH = 120;
            float ratio = std::min(static_cast<float>(maxW) / static_cast<float>(origW),
                                   static_cast<float>(maxH) / static_cast<float>(origH));
            if (ratio > 1.0f) ratio = 1.0f;
            outW = std::max(2, static_cast<int>(std::floor(origW * ratio)) & ~1);
            outH = std::max(1, static_cast<int>(std::floor(origH * ratio)));

            rowBytes = 1 + static_cast<size_t>(origW) * bpp;
            prevRow.assign(static_cast<size_t>(origW) * bpp, 0);
            curRow.resize(rowBytes);
            outPixels = static_cast<unsigned char *>(std::malloc(static_cast<size_t>(outW) * outH * 4));
            if (!outPixels) return cleanupFail();
            if (mz_inflateInit(&zs) != MZ_OK) return cleanupFail();
            zsInit = true;
        } else if (std::memcmp(type, "PLTE", 4) == 0) {
            uint32_t count = std::min<uint32_t>(len / 3, 256);
            for (uint32_t i = 0; i < count; ++i) {
                palette[i][0] = cdata[i * 3 + 0];
                palette[i][1] = cdata[i * 3 + 1];
                palette[i][2] = cdata[i * 3 + 2];
            }
        } else if (std::memcmp(type, "tRNS", 4) == 0 && colorType == 3) {
            uint32_t count = std::min<uint32_t>(len, 256);
            std::memcpy(trnsAlpha, cdata, count);
        } else if (std::memcmp(type, "IDAT", 4) == 0 && zsInit) {
            zs.next_in = const_cast<unsigned char *>(cdata);
            zs.avail_in = len;
            while (zs.avail_in > 0 && curY < origH) {
                zs.next_out = curRow.data() + curRowPos;
                zs.avail_out = static_cast<unsigned int>(rowBytes - curRowPos);
                int rc = mz_inflate(&zs, MZ_NO_FLUSH);
                curRowPos = rowBytes - zs.avail_out;
                if (curRowPos == rowBytes) {
                    uint8_t filter = curRow[0];
                    uint8_t *p = curRow.data() + 1;
                    const size_t rawBytes = rowBytes - 1;
                    if (filter == 1) {
                        for (size_t i = bpp; i < rawBytes; ++i) p[i] = static_cast<uint8_t>(p[i] + p[i - bpp]);
                    } else if (filter == 2) {
                        for (size_t i = 0; i < rawBytes; ++i) p[i] = static_cast<uint8_t>(p[i] + prevRow[i]);
                    } else if (filter == 3) {
                        for (size_t i = 0; i < rawBytes; ++i) {
                            uint8_t a = (i >= static_cast<size_t>(bpp)) ? p[i - bpp] : 0;
                            p[i] = static_cast<uint8_t>(p[i] + ((static_cast<int>(a) + static_cast<int>(prevRow[i])) >> 1));
                        }
                    } else if (filter == 4) {
                        for (size_t i = 0; i < rawBytes; ++i) {
                            uint8_t a = (i >= static_cast<size_t>(bpp)) ? p[i - bpp] : 0;
                            uint8_t b = prevRow[i];
                            uint8_t c = (i >= static_cast<size_t>(bpp)) ? prevRow[i - bpp] : 0;
                            p[i] = static_cast<uint8_t>(p[i] + paethPredictor(a, b, c));
                        }
                    } else if (filter != 0) {
                        return cleanupFail();
                    }

                    while (nextDy < outH && ((nextDy * origH) / outH) == curY) {
                        uint8_t *dst = outPixels + static_cast<size_t>(nextDy) * outW * 4;
                        for (int dx = 0; dx < outW; ++dx) {
                            int sx = (dx * origW) / outW;
                            const uint8_t *sp = p + static_cast<size_t>(sx) * bpp;
                            uint8_t *dp = dst + dx * 4;
                            if (colorType == 6) {
                                dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2]; dp[3] = sp[3];
                            } else if (colorType == 2) {
                                dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2]; dp[3] = 255;
                            } else if (colorType == 3) {
                                uint8_t idx = sp[0];
                                dp[0] = palette[idx][0]; dp[1] = palette[idx][1]; dp[2] = palette[idx][2]; dp[3] = trnsAlpha[idx];
                            } else if (colorType == 4) {
                                dp[0] = sp[0]; dp[1] = sp[0]; dp[2] = sp[0]; dp[3] = sp[1];
                            } else {
                                dp[0] = sp[0]; dp[1] = sp[0]; dp[2] = sp[0]; dp[3] = 255;
                            }
                        }
                        nextDy++;
                    }
                    std::memcpy(prevRow.data(), p, rawBytes);
                    curRowPos = 0;
                    curY++;
                }
                if (rc == MZ_STREAM_END) break;
                if (rc != MZ_OK && rc != MZ_BUF_ERROR) return cleanupFail();
            }
        } else if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }
        pos += 12 + static_cast<size_t>(len);
    }

    if (zsInit) mz_inflateEnd(&zs);
    if (!outPixels || curY == 0) {
        std::free(outPixels);
        return nullptr;
    }
    imgScale *= static_cast<float>(outW) / static_cast<float>(origW);
    return outPixels;
}
#endif

nonstd::expected<unsigned char *, std::string> Image::loadRasterFromMemory(const unsigned char *data, size_t size, int &width, int &height, bool bitmapHalfQuality,
                                                                           const unsigned char *textData, size_t textSize) {
#ifdef ENABLE_BITMAP
    int channels;
    unsigned char *pixels = stbi_load_from_memory(data, size, &width, &height, &channels, 4);
#if defined(__XBOX__) || defined(__VFLASH__)
    bool usedStreamingFallback = false;
    if (!pixels) {
        Scratch::freeUnusedCostumeImages(this);
        pixels = stbi_load_from_memory(data, size, &width, &height, &channels, 4);
    }
#ifdef __VFLASH__
    if (!pixels) {
        pixels = decodePngDownscaledVFlash(data, size, width, height, imgData.scale);
        usedStreamingFallback = (pixels != nullptr);
    }
#endif
    if (!pixels || width <= 0 || height <= 0) {
        if (pixels) stbi_image_free(pixels);
        const char *reason = stbi_failure_reason();
        return nonstd::make_unexpected(std::string("Failed to decode raster image: ") + (reason ? reason : "unknown"));
    }
#else
    if (!pixels) {
        return nonstd::make_unexpected("Failed to decode raster image.");
    }
#endif
    if (textData) {
        int textWidth = 0, textHeight = 0, textChannels = 0;
        unsigned char *text = stbi_load_from_memory(textData, textSize, &textWidth, &textHeight, &textChannels, 4);
#if defined(__XBOX__) || defined(__VFLASH__)
        if (!text) {
            Scratch::freeUnusedCostumeImages(this);
            text = stbi_load_from_memory(textData, textSize, &textWidth, &textHeight, &textChannels, 4);
        }
#endif
        if (!text || textWidth <= 0 || textHeight <= 0) {
            if (text) stbi_image_free(text);
            stbi_image_free(pixels);
            return nonstd::make_unexpected("Failed to decode SB2 bitmap text layer");
        }
        Sb2BitmapLayers::composite(pixels, width, height, text, textWidth, textHeight);
        stbi_image_free(text);
    }
    imgData.pitch = width * 4;

#ifdef __OGC__ // may break getPixels()
    stbi__vertical_flip(pixels, width, height, 4);
#endif

    unsigned char *resultPixels = pixels;
#ifdef __VFLASH__
    if (bitmapHalfQuality && !usedStreamingFallback) {
#else
    if (bitmapHalfQuality) {
#endif
        int resizedW, resizedH;
        unsigned char *resizedPixels = resizeRaster(resultPixels, width, height, resizedW, resizedH, this);
        if (!resizedPixels) {
            stbi_image_free(resultPixels);
            return nonstd::make_unexpected("Failed to allocate resized raster buffer");
        }

        imgData.pitch = resizedW * 4;
        width = resizedW;
        height = resizedH;

        stbi_image_free(resultPixels);
        resultPixels = resizedPixels;
    }

#if defined(__XBOX__)
    // Large images are split into tiles by Image_Xbox, so only cap the decoded size.
    while (width > 2048 || height > 2048) {
        int resizedW, resizedH;
        unsigned char *resizedPixels = resizeRaster(resultPixels, width, height, resizedW, resizedH, this);
        if (!resizedPixels) {
            if (resultPixels == pixels) {
                stbi_image_free(resultPixels);
            } else {
                free(resultPixels);
            }
            return nonstd::make_unexpected("Failed to allocate downscaled raster buffer");
        }

        imgData.pitch = resizedW * 4;
        width = resizedW;
        height = resizedH;

        if (resultPixels == pixels) {
            stbi_image_free(resultPixels);
        } else {
            free(resultPixels);
        }
        resultPixels = resizedPixels;
    }
#endif

    return resultPixels;
#endif
    width = 0;
    height = 0;
    return nullptr;
}

Image::Image(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) {
    auto potentialError = init(filePath, fromScratchProject, bitmapHalfQuality, scale);
    if (!potentialError.has_value()) error = potentialError.error();
}

Image::Image(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality, float scale) {
    auto potentialError = init(filePath, zip, bitmapHalfQuality, scale);
    if (!potentialError.has_value()) error = potentialError.error();
}

nonstd::expected<void, std::string> Image::init(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) {
    reloadInfo.path = filePath;
    reloadInfo.zip = nullptr;
    reloadInfo.useZip = false;
    reloadInfo.fromProject = fromScratchProject;
    reloadInfo.halfQuality = bitmapHalfQuality;
    reloadInfo.valid = true;
    const auto layers = fromScratchProject && Unzip::currentFormat == ProjectFormat::SB2
                            ? Sb2BitmapLayers::split(filePath)
                            : std::nullopt;
    if (layers) filePath = layers->first;
    const auto resolvePath = [fromScratchProject](const std::string &name) {
        if (fromScratchProject) {
            if (Unzip::UnpackedInSD) return Unzip::filePath + name;
#if defined(__XBOX__)
            return OS::getScratchFolderLocation() + "project/" + name;
#else
            return OS::getRomFSLocation() + "project/" + name;
#endif
        }
        return OS::getRomFSLocation() + name;
    };
    filePath = resolvePath(filePath);

    const bool isSVG = hasSVGExtension(filePath);
    isSVGImage = isSVG;

    auto buffer = readFileToBuffer(filePath, fromScratchProject);
    if (!buffer.has_value()) return nonstd::make_unexpected(buffer.error());
    std::vector<unsigned char> textBuffer;
    if (layers) {
        auto text = readFileToBuffer(resolvePath(layers->second), fromScratchProject);
        if (!text.has_value()) return nonstd::make_unexpected(text.error());
        textBuffer = std::move(text.value());
    }

    if (isSVG) {
        auto pixels = loadSVGFromMemory(reinterpret_cast<const char *>(buffer.value().data()), buffer.value().size(), imgData.width, imgData.height, scale,
                                        fromScratchProject && Unzip::currentFormat == ProjectFormat::SB2);
        if (!pixels.has_value()) return nonstd::make_unexpected(pixels.error());
        imgData.pixels = pixels.value();
    } else {
        auto pixels = loadRasterFromMemory(buffer.value().data(), buffer.value().size(), imgData.width, imgData.height, bitmapHalfQuality,
                                           textBuffer.empty() ? nullptr : textBuffer.data(), textBuffer.size());
        if (!pixels.has_value()) return nonstd::make_unexpected(pixels.error());
        imgData.pixels = pixels.value();
    }

    if (!imgData.pixels) return nonstd::make_unexpected("Failed to load image: " + filePath);

    return {};
}

nonstd::expected<void, std::string> Image::init(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality, float scale) {
    reloadInfo.path = filePath;
    reloadInfo.zip = zip;
    reloadInfo.useZip = true;
    reloadInfo.halfQuality = bitmapHalfQuality;
    reloadInfo.valid = true;
    const auto layers = Unzip::currentFormat == ProjectFormat::SB2 ? Sb2BitmapLayers::split(filePath) : std::nullopt;
    if (layers) filePath = layers->first;
    void *raw_data = nullptr;
    size_t file_size = 0;
    if (zip != nullptr) {
        raw_data = zip->extractToHeap(filePath, &file_size);
#if defined(__XBOX__)
        if (!raw_data) {
            Scratch::freeUnusedCostumeImages(this);
            raw_data = zip->extractToHeap(filePath, &file_size);
        }
#endif
    } else {
        raw_data = Unzip::getFileInSB3(filePath, &file_size);
#if defined(__XBOX__)
        if (!raw_data) {
            Scratch::freeUnusedCostumeImages(this);
            raw_data = Unzip::getFileInSB3(filePath, &file_size);
        }
#endif
    }

    if (!raw_data || file_size == 0) {
        if (raw_data) {
            if (zip) zip->freeHeap(raw_data);
            else free(raw_data);
        }
        return nonstd::make_unexpected("Failed to extract: " + filePath);
    }

    struct ZipHeapGuard {
        ZipArchive *zip;
        void *ptr;
        ~ZipHeapGuard() {
            if (!ptr) return;
            if (zip) zip->freeHeap(ptr);
            else free(ptr);
        }
    } file_data{zip, raw_data};
    ZipHeapGuard text_data{zip, nullptr};
    size_t text_size = 0;
    if (layers) {
        const auto extractText = [&]() {
            return zip ? zip->extractToHeap(layers->second, &text_size) : Unzip::getFileInSB3(layers->second, &text_size);
        };
        text_data.ptr = extractText();
#ifdef __XBOX__
        if (!text_data.ptr) {
            Scratch::freeUnusedCostumeImages(this);
            text_data.ptr = extractText();
        }
#endif
        if (!text_data.ptr || text_size == 0) return nonstd::make_unexpected("Failed to extract SB2 text layer: " + layers->second);
    }

    const bool isSVG = hasSVGExtension(filePath);
    isSVGImage = isSVG;

    if (isSVG) {
        std::vector<unsigned char> buffer(file_size + 1);
        memcpy(buffer.data(), raw_data, file_size);
        buffer[file_size] = '\0';

        auto pixels = loadSVGFromMemory(reinterpret_cast<const char *>(buffer.data()), file_size, imgData.width, imgData.height, scale,
                                        Unzip::currentFormat == ProjectFormat::SB2);
        if (!pixels.has_value()) return nonstd::make_unexpected(pixels.error());
        imgData.pixels = pixels.value();
    } else {
        auto pixels = loadRasterFromMemory((unsigned char *)raw_data, file_size, imgData.width, imgData.height, bitmapHalfQuality,
                                           static_cast<const unsigned char *>(text_data.ptr), text_size);
        if (!pixels.has_value()) return nonstd::make_unexpected(pixels.error());
        imgData.pixels = pixels.value();
    }

    if (!imgData.pixels) return nonstd::make_unexpected("Failed to load image from zip: " + filePath);

    return {};
}

void Image::discardColorData() {
    if (!imgData.pixels || !reloadInfo.valid || imgData.width <= 0 || imgData.height <= 0) return;

    if (!isOpaque) {
        alpha8.resize(static_cast<size_t>(imgData.width) * imgData.height);
        for (int y = 0; y < imgData.height; y++) {
            const uint8_t *row = static_cast<const uint8_t *>(imgData.pixels) + static_cast<size_t>(y) * imgData.pitch;
            uint8_t *out = alpha8.data() + static_cast<size_t>(y) * imgData.width;
            for (int x = 0; x < imgData.width; x++)
                out[x] = row[x * 4 + 3];
        }
    }
    freePixels();
}

bool Image::ensureColorData() {
    if (imgData.pixels) return true;
    if (colorDataUnavailable || !reloadInfo.valid) return false;

    const ImageData saved = imgData;
    const auto info = reloadInfo;
    auto result = info.useZip ? init(info.path, info.zip, info.halfQuality, saved.scale)
                              : init(info.path, info.fromProject, info.halfQuality, saved.scale);
    if (!result.has_value() || !imgData.pixels || imgData.width != saved.width || imgData.height != saved.height) {
        freePixels();
        imgData = saved;
        colorDataUnavailable = true;
        Log::logWarning("Unable to restore color data: " + info.path);
        return false;
    }
    imgData.scale = saved.scale;
    freeTimer = maxFreeTimer;
    return true;
}

Image::~Image() {
    if (imgData.pixels)
        free(imgData.pixels);
}

int Image::getWidth() {
    return imgData.width / imgData.scale;
}

int Image::getHeight() {
    return imgData.height / imgData.scale;
}

float Image::getExactWidth() {
    if (logicalSizeKnown) return std::max(logicalWidth, 0.0f);
    return logicalWidth > 0.0f ? logicalWidth : static_cast<float>(imgData.width) / imgData.scale;
}

float Image::getExactHeight() {
    if (logicalSizeKnown) return std::max(logicalHeight, 0.0f);
    return logicalHeight > 0.0f ? logicalHeight : static_cast<float>(imgData.height) / imgData.scale;
}

ImageData Image::getPixels(ImageSubrect rect) {
    ImageData out{};

    if (rect.x < 0) rect.x = 0;
    if (rect.y < 0) rect.y = 0;
    if (rect.x + rect.w > imgData.width) rect.w = imgData.width - rect.x;
    if (rect.y + rect.h > imgData.height) rect.h = imgData.height - rect.y;

    if (rect.w <= 0 || rect.h <= 0 || !imgData.pixels) {
        out.format = IMAGE_FORMAT_NONE;
        return out;
    }

    const int bytesPerPixel = 4;
    unsigned char *base =
        static_cast<unsigned char *>(imgData.pixels) +
        rect.y * imgData.pitch +
        rect.x * bytesPerPixel;

    out.width = rect.w;
    out.height = rect.h;
    out.scale = imgData.scale;
    out.format = IMAGE_FORMAT_RGBA32;
    out.pitch = imgData.pitch;
    out.pixels = base;

    return out;
}
