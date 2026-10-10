#include "image.hpp"
#include "nonstd/expected.hpp"
#include "os.hpp"
#if defined(__XBOX__)
#include <log.hpp>
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
#endif

#if defined(RENDERER_SDL1)
#include <image_sdl1.hpp>
#elif defined(RENDERER_SDL2)
#include <image_sdl2.hpp>
#elif defined(RENDERER_SDL3)
#include <image_sdl3.hpp>
#elif defined(RENDERER_OPENGL)
#include <image_gl.hpp>
#elif defined(RENDERER_CITRO2D)
#include <image_c2d.hpp>
#elif defined(RENDERER_GL2D)
#include <image_gl2d.hpp>
#elif defined(RENDERER_OPENGL_CORE)
#include <image_gl_core.hpp>
#elif defined(RENDERER_GDI)
#include <image_gdi.hpp>
#elif defined(RENDERER_HEADLESS)
#include <image_headless.hpp>
#endif

#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
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
#if defined(__XBOX__)
        Log::logError("loadFont: family '" + family + "' not found in loadedFonts");
#endif
        return false;
    }
    if (it->second.isLoaded) return true;

    const std::string &path = it->second.path;
#if defined(__XBOX__)
    Log::log("loadFont: loading font family='" + family + "' path=" + path);
#endif

#ifdef USE_CMAKERC
#if defined(__XBOX__)
    std::string cmrcFontPath = OS::normalizeCMRCPath(OS::getRomFSLocation() + path + ".ttf");
    auto fs = cmrc::romfs::get_filesystem();
    if (!fs.exists(cmrcFontPath)) {
        Log::logError("loadFont: CMRC font file not found: " + cmrcFontPath);
        return false;
    }
    const auto &file = fs.open(cmrcFontPath);
    if (!lunasvg_add_font_face_from_data(family.c_str(), false, false, file.begin(), file.size(), nullptr, nullptr)) {
        Log::logError("lunasvg_add_font_face_from_data failed for " + family);
        return false;
    }
#else
    const auto &file = cmrc::romfs::get_filesystem().open((OS::getRomFSLocation() + path + ".ttf").c_str());
    if (!lunasvg_add_font_face_from_data(family.c_str(), false, false, file.begin(), file.size(), nullptr, nullptr)) return false;
#endif
#else
#if defined(__XBOX__)
    if (!lunasvg_add_font_face_from_file(family.c_str(), false, false, (OS::getRomFSLocation() + path + ".ttf").c_str())) {
        Log::logError("lunasvg_add_font_face_from_file failed for " + family);
        return false;
    }
#else
    if (!lunasvg_add_font_face_from_file(family.c_str(), false, false, (OS::getRomFSLocation() + path + ".ttf").c_str())) return false;
#endif
#endif

    it->second.isLoaded = true;
#if defined(__XBOX__)
    Log::log("loadFont: successfully loaded font family='" + family + "'");
#endif
    return true;
#endif
    return false;
}

nonstd::expected<std::shared_ptr<Image>, std::string> createImageFromFile(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality, float scale) {
#if defined(__XBOX__)
    Log::log("createImageFromFile: filePath=" + filePath);
#endif
    auto it = images.find(filePath);
    if (it != images.end()) {
        if (auto img = it->second.lock()) {
            return img;
        } else {
            images.erase(it);
        }
    }

#if defined(RENDERER_SDL1)
    Image *rawImg = new Image_SDL1(filePath, fromScratchProject, bitmapHalfQuality, scale);
#elif defined(RENDERER_SDL2)
    Image *rawImg = new Image_SDL2(filePath, fromScratchProject, bitmapHalfQuality, scale);
#elif defined(RENDERER_SDL3)
    Image *rawImg = new Image_SDL3(filePath, fromScratchProject, bitmapHalfQuality, scale);
#elif defined(RENDERER_CITRO2D)
    Image *rawImg = new Image_C2D(filePath, fromScratchProject, bitmapHalfQuality, scale);
#elif defined(RENDERER_OPENGL)
    Image *rawImg = new Image_GL(filePath, fromScratchProject, bitmapHalfQuality, scale);
#elif defined(RENDERER_GL2D)
    Image *rawImg = new Image_GL2D(filePath, fromScratchProject, bitmapHalfQuality, scale);
#elif defined(RENDERER_OPENGL_CORE)
    Image *rawImg = new Image_GLCore(filePath, fromScratchProject, bitmapHalfQuality, scale);
#elif defined(RENDERER_GDI)
    Image *rawImg = new Image_GDI(filePath, fromScratchProject, bitmapHalfQuality, scale);
#elif defined(RENDERER_HEADLESS)
    Image *rawImg = new Image_Headless(filePath, fromScratchProject, bitmapHalfQuality, scale);
#else
#error "Image backend not defined."
#endif

    if (rawImg->error.has_value()) {
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

#if defined(RENDERER_SDL1)
    Image *rawImg = new Image_SDL1(filePath, zip, bitmapHalfQuality, scale);
#elif defined(RENDERER_SDL2)
    Image *rawImg = new Image_SDL2(filePath, zip, bitmapHalfQuality, scale);
#elif defined(RENDERER_SDL3)
    Image *rawImg = new Image_SDL3(filePath, zip, bitmapHalfQuality, scale);
#elif defined(RENDERER_CITRO2D)
    Image *rawImg = new Image_C2D(filePath, zip, bitmapHalfQuality, scale);
#elif defined(RENDERER_OPENGL)
    Image *rawImg = new Image_GL(filePath, zip, bitmapHalfQuality, scale);
#elif defined(RENDERER_GL2D)
    Image *rawImg = new Image_GL2D(filePath, zip, bitmapHalfQuality, scale);
#elif defined(RENDERER_OPENGL_CORE)
    Image *rawImg = new Image_GLCore(filePath, zip, bitmapHalfQuality, scale);
#elif defined(RENDERER_GDI)
    Image *rawImg = new Image_GDI(filePath, zip, bitmapHalfQuality, scale);
#elif defined(RENDERER_HEADLESS)
    Image *rawImg = new Image_Headless(filePath, zip, bitmapHalfQuality, scale);
#else
#error "Image backend not defined."
#endif

    if (rawImg->error.has_value()) {
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
#if defined(__XBOX__)
    Log::log("readFileToBuffer: filePath=" + filePath + " fromScratchProject=" + std::to_string(fromScratchProject));
#endif
#ifdef USE_CMAKERC
    if (!Unzip::UnpackedInSD || !fromScratchProject) {
        auto fs = cmrc::romfs::get_filesystem();
#if defined(__XBOX__)
        std::string cmrcPath = OS::normalizeCMRCPath(filePath);
        Log::log("readFileToBuffer (CMRC): cmrcPath=" + cmrcPath + " exists=" + std::to_string(fs.exists(cmrcPath)));
        if (!fs.exists(cmrcPath)) return nonstd::make_unexpected("File not found: " + filePath);
        auto file = fs.open(cmrcPath);
        Log::log("readFileToBuffer (CMRC): opened size=" + std::to_string(file.size()));
#else
        if (!fs.exists(filePath)) return nonstd::make_unexpected("File not found: " + filePath);
        auto file = fs.open(filePath);
#endif
        std::vector<unsigned char> buffer(file.size() + 1);
        std::copy(file.begin(), file.end(), buffer.begin());
        buffer[file.size()] = '\0';
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

#if defined(__XBOX__)
static std::string safe_float_to_string(float val) {
    int i = (int)val;
    int f = (int)std::abs((val - i) * 100.0f);
    return std::to_string(i) + "." + (f < 10 ? "0" : "") + std::to_string(f);
}
#endif

nonstd::expected<unsigned char *, std::string> Image::loadSVGFromMemory(const char *data, size_t size, int &width, int &height, float scale) {
#ifdef ENABLE_SVG
#if defined(__XBOX__)
    Log::log("loadSVGFromMemory: data size=" + std::to_string(size) + " scale=" + safe_float_to_string(scale));
#endif
    if constexpr (maxScale != 0)
        if (scale > maxScale) scale = maxScale;

    const std::string_view svgView(data, size);

    // always load default font if there is text present
    if (svgView.find("<text") != std::string_view::npos || svgView.find("<tspan") != std::string_view::npos) {
#if defined(__XBOX__)
        Log::log("SVG contains text, loading font...");
#endif
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

    Log::log("Parsing SVG data of length " + std::to_string(svgData.size()));
    Log::log("Calling lunasvg::Document::loadFromData...");
    svgDocument = lunasvg::Document::loadFromData(svgData.c_str());
    Log::log("lunasvg::Document::loadFromData returned ptr=" + std::to_string(reinterpret_cast<uintptr_t>(svgDocument.get())));
    if (!svgDocument) {
        Log::logError("LunaSVG failed to parse SVG data of size " + std::to_string(size));
        return nonstd::make_unexpected("LunaSVG failed to parse SVG");
    }

    float docW = svgDocument->width();
    float docH = svgDocument->height();
    Log::log("SVG dimensions: w=" + safe_float_to_string(docW) + " h=" + safe_float_to_string(docH));

    const float targetWidth = docW * scale;
    const float targetHeight = docH * scale;
#else
    svgDocument = lunasvg::Document::loadFromData(std::string(data, size).c_str());
    if (!svgDocument) return nonstd::make_unexpected("LunaSVG failed to parse SVG");

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

    Log::log("Rendering SVG to bitmap: w=" + std::to_string(width) + " h=" + std::to_string(height));
    auto bitmap = svgDocument->renderToBitmap(width, height);
    if (!bitmap.valid()) {
        Log::logError("LunaSVG failed to render SVG to bitmap");
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
    if (!dst) return nonstd::make_unexpected("Failed to allocate SVG pixels buffer");

    reorderPixels(src, dst, pixelsSize);

    imgData.pitch = width * 4;
#if defined(__XBOX__)
    Log::log("SVG decoded successfully: w=" + std::to_string(width) + " h=" + std::to_string(height));
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
    outW = srcW >> 1;
    outH = srcH >> 1;

    if (outW <= 0) outW = 1;
    if (outH <= 0) outH = 1;

    unsigned char *dst = (unsigned char *)malloc(outW * outH * 4);

    const int srcStride = srcW * 4;
    const int dstStride = outW * 4;

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
#if defined(__XBOX__)
    Log::log("loadRasterFromMemory: size=" + std::to_string(size) + " calling stbi_load_from_memory...");
#endif
    int channels;
    unsigned char *pixels = stbi_load_from_memory(data, size, &width, &height, &channels, 4);
#if defined(__XBOX__)
    Log::log("loadRasterFromMemory: stbi_load_from_memory returned pixels=" + std::to_string((uintptr_t)pixels) + " w=" + std::to_string(width) + " h=" + std::to_string(height));
    if (!pixels) {
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

    if (bitmapHalfQuality) {
        int resizedW, resizedH;
        unsigned char *resizedPixels = resizeRaster(pixels, width, height, resizedW, resizedH);

        imgData.pitch = resizedW * 4;
        width = resizedW;
        height = resizedH;

        stbi_image_free(pixels);

        return resizedPixels;
    } else {
        return pixels;
    }
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
        else filePath = OS::getRomFSLocation() + "project/" + filePath;
    } else filePath = OS::getRomFSLocation() + filePath;

    bool isSVG = filePath.size() >= 4 && (filePath.substr(filePath.size() - 4) == ".svg" || filePath.substr(filePath.size() - 4) == ".SVG");

#if defined(__XBOX__)
    Log::log("Image::init: filePath=" + filePath + " isSVG=" + std::to_string(isSVG));
#endif

    auto buffer = readFileToBuffer(filePath, fromScratchProject);
    if (!buffer.has_value()) return nonstd::make_unexpected(buffer.error());

    if (isSVG) {
        auto pixels = loadSVGFromMemory(reinterpret_cast<const char *>(buffer.value().data()), buffer.value().size(), imgData.width, imgData.height);
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
    } else {
        raw_data = Unzip::getFileInSB3(filePath, &file_size);
    }

    if (!raw_data) return nonstd::make_unexpected("Failed to extract: " + filePath);

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

        auto pixels = loadSVGFromMemory(reinterpret_cast<const char *>(buffer.data()), file_size, imgData.width, imgData.height, scale);
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
