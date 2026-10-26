#include "image.hpp"
#include "nonstd/expected.hpp"
#include "os.hpp"
#include "renderer.hpp"
#include <cstdlib>
#include <log.hpp>
#ifdef __XBOX__
#include <runtime.hpp>
#endif
#include <stdexcept>
#include <string_view>
#include <unzip.hpp>
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
#endif

#if defined(RENDERER_SDL1)
#include <image_sdl1.hpp>
#endif
#if defined(RENDERER_SDL2)
#include <image_sdl2.hpp>
#endif
#if defined(RENDERER_XBOX)
#include <image_xbox.hpp>
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

    const std::string &path = it->second.path;

#ifdef USE_BUNDLE
    auto entry = Bundle::assets().find(OS::getRomFSLocation() + path + ".ttf");
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
#if defined(RENDERER_XBOX)
    case Renderers::Xbox:
        rawImg = new Image_Xbox(filePath, fromScratchProject, bitmapHalfQuality, scale);
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
#if defined(RENDERER_XBOX)
    case Renderers::Xbox:
        rawImg = new Image_Xbox(filePath, zip, bitmapHalfQuality, scale);
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

    float docW = svgDocument->width();
    float docH = svgDocument->height();

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

    float docW = svgDocument->width();
    float docH = svgDocument->height();

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

    auto bitmap = svgDocument->renderToBitmap(width, height);
    if (!bitmap.valid()) {
        svgDocument.reset();
        return nonstd::make_unexpected("LunaSVG failed to render SVG to bitmap");
    }
#else
    width = std::max(1, (int)(svgDocument->width() * finalScale));
    height = std::max(1, (int)(svgDocument->height() * finalScale));
    imgData.scale = finalScale;

    auto bitmap = svgDocument->renderToBitmap(width, height);
    if (!bitmap.valid()) return nonstd::make_unexpected("LunaSVG failed to render SVG to bitmap");
#endif

    unsigned char *src = bitmap.data();
    const size_t pixelsSize = width * height * 4;
    unsigned char *dst = (unsigned char *)malloc(pixelsSize);
#ifdef __XBOX__
    if (!dst) {
        Scratch::freeUnusedCostumeImages();
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

    if (!svgDocument || scale <= imgData.scale) return {};
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

    imgData.width = width;
    imgData.height = height;
    imgData.scale = finalScale;

    auto bitmap = svgDocument->renderToBitmap(width, height);
    if (!bitmap.valid()) return nonstd::make_unexpected("LunaSVG failed to render SVG to bitmap");

    unsigned char *src = bitmap.data();
    const size_t pixelsSize = width * height * 4;
    unsigned char *dst = (unsigned char *)malloc(pixelsSize);
#ifdef __XBOX__
    if (!dst) {
        Scratch::freeUnusedCostumeImages();
        dst = (unsigned char *)malloc(pixelsSize);
    }
#endif
    if (!dst) return nonstd::make_unexpected("Failed to allocate SVG pixels buffer");

    reorderPixels(src, dst, pixelsSize);

    if (imgData.pixels != nullptr) free(imgData.pixels);

    imgData.pitch = width * 4;
    imgData.pixels = dst;

    return refreshTexture();
#endif

    return {};
}

unsigned char *Image::resizeRaster(const unsigned char *srcPixels, int srcW, int srcH, int &outW, int &outH) {
    if (srcW <= 0 || srcH <= 0 || !srcPixels) return nullptr;

    outW = srcW >> 1;
    outH = srcH >> 1;

    if (outW <= 0) outW = 1;
    if (outH <= 0) outH = 1;

    unsigned char *dst = (unsigned char *)malloc(outW * outH * 4);
#ifdef __XBOX__
    if (!dst) {
        Scratch::freeUnusedCostumeImages();
        dst = (unsigned char *)malloc(outW * outH * 4);
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

nonstd::expected<unsigned char *, std::string> Image::loadRasterFromMemory(const unsigned char *data, size_t size, int &width, int &height, bool bitmapHalfQuality) {
#ifdef ENABLE_BITMAP
    int channels;
    unsigned char *pixels = stbi_load_from_memory(data, size, &width, &height, &channels, 4);
#if defined(__XBOX__)
    if (!pixels) {
        Scratch::freeUnusedCostumeImages();
        pixels = stbi_load_from_memory(data, size, &width, &height, &channels, 4);
    }
    if (!pixels || width <= 0 || height <= 0) {
        if (pixels) stbi_image_free(pixels);
        const char *reason = stbi_failure_reason();
        return nonstd::make_unexpected(std::string("Failed to decode raster image: ") + (reason ? reason : "unknown"));
    }
#else
    if (!pixels) return nonstd::make_unexpected("Failed to decode raster image");
#endif
    imgData.pitch = width * 4;

#ifdef __OGC__ // may break getPixels()
    stbi__vertical_flip(pixels, width, height, 4);
#endif

    unsigned char *resultPixels = pixels;
    if (bitmapHalfQuality) {
        int resizedW, resizedH;
        unsigned char *resizedPixels = resizeRaster(resultPixels, width, height, resizedW, resizedH);
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
        unsigned char *resizedPixels = resizeRaster(resultPixels, width, height, resizedW, resizedH);
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
    if (fromScratchProject) {
        if (Unzip::UnpackedInSD) filePath = Unzip::filePath + filePath;
#if defined(__XBOX__)
        else filePath = OS::getScratchFolderLocation() + "project/" + filePath;
#else
        else filePath = OS::getRomFSLocation() + "project/" + filePath;
#endif
    } else filePath = OS::getRomFSLocation() + filePath;

    bool isSVG = filePath.size() >= 4 && (filePath.substr(filePath.size() - 4) == ".svg" || filePath.substr(filePath.size() - 4) == ".SVG");

    auto buffer = readFileToBuffer(filePath, fromScratchProject);
    if (!buffer.has_value()) return nonstd::make_unexpected(buffer.error());

    if (isSVG) {
        auto pixels = loadSVGFromMemory(reinterpret_cast<const char *>(buffer.value().data()), buffer.value().size(), imgData.width, imgData.height, 1,
                                        fromScratchProject && Unzip::currentFormat == ProjectFormat::SB2);
        if (!pixels.has_value()) return nonstd::make_unexpected(pixels.error());
        imgData.pixels = pixels.value();
    } else {
        auto pixels = loadRasterFromMemory(buffer.value().data(), buffer.value().size(), imgData.width, imgData.height, bitmapHalfQuality);
        if (!pixels.has_value()) return nonstd::make_unexpected(pixels.error());
        imgData.pixels = pixels.value();
    }

    if (!imgData.pixels) return nonstd::make_unexpected("Failed to load image: " + filePath);

    return {};
}

nonstd::expected<void, std::string> Image::init(std::string filePath, ZipArchive *zip, bool bitmapHalfQuality, float scale) {
    void *raw_data = nullptr;
    size_t file_size = 0;
    if (zip != nullptr) {
        raw_data = zip->extractToHeap(filePath, &file_size);
#if defined(__XBOX__)
        if (!raw_data) {
            Scratch::freeUnusedCostumeImages();
            raw_data = zip->extractToHeap(filePath, &file_size);
        }
#endif
    } else {
        raw_data = Unzip::getFileInSB3(filePath, &file_size);
#if defined(__XBOX__)
        if (!raw_data) {
            Scratch::freeUnusedCostumeImages();
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
            if (zip) zip->freeHeap(ptr);
            else free(ptr);
        }
    } file_data{zip, raw_data};

    bool isSVG = filePath.size() >= 4 &&
                 (filePath.substr(filePath.size() - 4) == ".svg" ||
                  filePath.substr(filePath.size() - 4) == ".SVG");

    if (isSVG) {
        std::vector<unsigned char> buffer(file_size + 1);
        memcpy(buffer.data(), raw_data, file_size);
        buffer[file_size] = '\0';

        auto pixels = loadSVGFromMemory(reinterpret_cast<const char *>(buffer.data()), file_size, imgData.width, imgData.height, scale,
                                        Unzip::currentFormat == ProjectFormat::SB2);
        if (!pixels.has_value()) return nonstd::make_unexpected(pixels.error());
        imgData.pixels = pixels.value();
    } else {
        auto pixels = loadRasterFromMemory((unsigned char *)raw_data, file_size, imgData.width, imgData.height, bitmapHalfQuality);
        if (!pixels.has_value()) return nonstd::make_unexpected(pixels.error());
        imgData.pixels = pixels.value();
    }

    if (!imgData.pixels) return nonstd::make_unexpected("Failed to load image from zip: " + filePath);

    return {};
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
