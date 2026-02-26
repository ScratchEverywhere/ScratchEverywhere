#include "image.hpp"
#include <stdexcept>
#include <unzip.hpp>
#ifdef __WIIU__
#define STBI_NO_THREAD_LOCALS
#endif
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <lunasvg.h>

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
#elif defined(RENDERER_HEADLESS)
#include <image_headless.hpp>
#endif

#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

std::unordered_map<std::string, std::weak_ptr<Image>> images;

std::shared_ptr<Image> createImageFromFile(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality) {
    auto it = images.find(filePath);
    if (it != images.end()) {
        if (auto img = it->second.lock()) {
            return img;
        } else {
            images.erase(it);
        }
    }

#if defined(RENDERER_SDL1)
    Image *rawImg = new Image_SDL1(filePath, fromScratchProject, bitmapHalfQuality);
#elif defined(RENDERER_SDL2)
    Image *rawImg = new Image_SDL2(filePath, fromScratchProject, bitmapHalfQuality);
#elif defined(RENDERER_SDL3)
    Image *rawImg = new Image_SDL3(filePath, fromScratchProject, bitmapHalfQuality);
#elif defined(RENDERER_CITRO2D)
    Image *rawImg = new Image_C2D(filePath, fromScratchProject, bitmapHalfQuality);
#elif defined(RENDERER_OPENGL)
    Image *rawImg = new Image_GL(filePath, fromScratchProject, bitmapHalfQuality);
#elif defined(RENDERER_GL2D)
    Image *rawImg = new Image_GL2D(filePath, fromScratchProject, bitmapHalfQuality);
#elif defined(RENDERER_HEADLESS)
    Image *rawImg = new Image_Headless(filePath, fromScratchProject, bitmapHalfQuality);
#else
#error "Image backend not defined."
#endif

    auto img = std::shared_ptr<Image>(rawImg, [filePath](Image *p) {
        images.erase(filePath);
        delete p;
    });

    images[filePath] = img;
    return img;
}

std::shared_ptr<Image> createImageFromZip(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality) {
    auto it = images.find(filePath);
    if (it != images.end()) {
        if (auto img = it->second.lock()) {
            return img;
        } else {
            images.erase(it);
        }
    }

#if defined(RENDERER_SDL1)
    Image *rawImg = new Image_SDL1(filePath, zip, bitmapHalfQuality);
#elif defined(RENDERER_SDL2)
    Image *rawImg = new Image_SDL2(filePath, zip, bitmapHalfQuality);
#elif defined(RENDERER_SDL3)
    Image *rawImg = new Image_SDL3(filePath, zip, bitmapHalfQuality);
#elif defined(RENDERER_CITRO2D)
    Image *rawImg = new Image_C2D(filePath, zip, bitmapHalfQuality);
#elif defined(RENDERER_OPENGL)
    Image *rawImg = new Image_GL(filePath, zip, bitmapHalfQuality);
#elif defined(RENDERER_GL2D)
    Image *rawImg = new Image_GL2D(filePath, zip, bitmapHalfQuality);
#elif defined(RENDERER_HEADLESS)
    Image *rawImg = new Image_Headless(filePath, zip, bitmapHalfQuality);
#else
#error "Image backend not defined."
#endif

    auto img = std::shared_ptr<Image>(rawImg, [filePath](Image *p) {
        images.erase(filePath);
        delete p;
    });

    images[filePath] = img;
    return img;
}

std::vector<char> Image::readFileToBuffer(const std::string &filePath, bool fromScratchProject) {
#ifdef USE_CMAKERC
    if (!Unzip::UnpackedInSD || !fromScratchProject) {
        auto file = cmrc::romfs::get_filesystem().open(filePath);
        std::vector<char> buffer(file.size() + 1);
        std::copy(file.begin(), file.end(), buffer.begin());
        buffer[file.size()] = '\0';
        return buffer;
    }
#endif

    std::string path = filePath;

    FILE *file = fopen(path.c_str(), "rb");
    if (!file) throw std::runtime_error("Failed to open file: " + path);

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0) {
        fclose(file);
        throw std::runtime_error("Empty file: " + path);
    }

    std::vector<char> buffer(size + 1);
    if (fread(buffer.data(), 1, size, file) != (size_t)size) {
        fclose(file);
        throw std::runtime_error("Failed to read file: " + path);
    }
    buffer[size] = '\0';
    fclose(file);
    return buffer;
}

unsigned char *Image::loadSVGFromMemory(const char *data, size_t size, int &width, int &height) {
    auto document = lunasvg::Document::loadFromData(std::string(data, size).c_str());
    if (!document) throw std::runtime_error("LunaSVG failed to parse SVG");

    width = document->width();
    height = document->height();

    if (width <= 0) width = 32;
    if (height <= 0) height = 32;

    auto bitmap = document->renderToBitmap(width, height);
    if (!bitmap.valid()) throw std::runtime_error("LunaSVG failed to render SVG to bitmap");

    unsigned char *src = bitmap.data();
    const size_t pixelsSize = width * height * 4;
    unsigned char *dst = (unsigned char *)malloc(pixelsSize);
    if (!dst) throw std::runtime_error("Failed to allocate SVG pixels buffer");

    for (size_t i = 0; i < pixelsSize; i += 4) {
        dst[i + 0] = src[i + 2];
        dst[i + 1] = src[i + 1];
        dst[i + 2] = src[i + 0];
        dst[i + 3] = src[i + 3];
    }

    imgData.pitch = width * 4;

    return dst;
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

unsigned char *Image::loadRasterFromMemory(const unsigned char *data, size_t size, int &width, int &height, bool bitmapHalfQuality) {
    int channels;
    unsigned char *pixels = stbi_load_from_memory(data, size, &width, &height, &channels, 4);
    if (!pixels) throw std::runtime_error("Failed to decode raster image");
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
}

Image::Image(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality) {

    if (fromScratchProject) {
        if (Unzip::UnpackedInSD) filePath = Unzip::filePath + filePath;
        else filePath = OS::getRomFSLocation() + "project/" + filePath;
    } else filePath = OS::getRomFSLocation() + filePath;

    bool isSVG = filePath.size() >= 4 && (filePath.substr(filePath.size() - 4) == ".svg" || filePath.substr(filePath.size() - 4) == ".SVG");

    std::vector<char> buffer = readFileToBuffer(filePath, fromScratchProject);

    if (isSVG) {
        imgData.pixels = loadSVGFromMemory(buffer.data(), buffer.size(), imgData.width, imgData.height);
    } else {
        imgData.pixels = loadRasterFromMemory(reinterpret_cast<unsigned char *>(buffer.data()), buffer.size(), imgData.width, imgData.height, bitmapHalfQuality);
    }

    if (!imgData.pixels) throw std::runtime_error("Failed to load image: " + filePath);
}

Image::Image(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality) {
    int file_index = mz_zip_reader_locate_file(zip, filePath.c_str(), nullptr, 0);
    if (file_index < 0) throw std::runtime_error("Image not found in SB3: " + filePath);

    size_t file_size;
    void *file_data = mz_zip_reader_extract_to_heap(zip, file_index, &file_size, 0);
    if (!file_data) throw std::runtime_error("Failed to extract: " + filePath);

    bool isSVG = filePath.size() >= 4 &&
                 (filePath.substr(filePath.size() - 4) == ".svg" ||
                  filePath.substr(filePath.size() - 4) == ".SVG");

    if (isSVG) {
        std::vector<char> svgBuffer(file_size + 1);
        memcpy(svgBuffer.data(), file_data, file_size);
        svgBuffer[file_size] = '\0';

        imgData.pixels = loadSVGFromMemory(svgBuffer.data(), file_size, imgData.width, imgData.height);
    } else {
        imgData.pixels = loadRasterFromMemory((unsigned char *)file_data, file_size, imgData.width, imgData.height, bitmapHalfQuality);
    }

    mz_free(file_data);

    if (!imgData.pixels) throw std::runtime_error("Failed to load image from zip: " + filePath);
}

Image::~Image() {
    if (imgData.pixels)
        stbi_image_free(imgData.pixels);
}

int Image::getWidth() {
    return imgData.width;
}

int Image::getHeight() {
    return imgData.height;
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
    out.format = IMAGE_FORMAT_RGBA32;
    out.pitch = imgData.pitch;
    out.pixels = base;

    return out;
}
