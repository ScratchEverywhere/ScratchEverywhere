#include "image.hpp"
#include <stdexcept>
#include <unzip.hpp>
#define STBI_NO_GIF
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define NANOSVG_IMPLEMENTATION
#include <nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvgrast.h>

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

std::shared_ptr<Image> createImageFromFile(std::string filePath, bool fromScratchProject) {
    auto it = images.find(filePath);
    if (it != images.end()) {
        if (auto img = it->second.lock()) {
            return img;
        } else {
            images.erase(it);
        }
    }

#if defined(RENDERER_SDL1)
    Image *rawImg = new Image_SDL1(filePath, fromScratchProject);
#elif defined(RENDERER_SDL2)
    Image *rawImg = new Image_SDL2(filePath, fromScratchProject);
#elif defined(RENDERER_SDL3)
    Image *rawImg = new Image_SDL3(filePath, fromScratchProject);
#elif defined(RENDERER_CITRO2D)
    Image *rawImg = new Image_C2D(filePath, fromScratchProject);
#elif defined(RENDERER_OPENGL)
    Image *rawImg = new Image_GL(filePath, fromScratchProject);
#elif defined(RENDERER_GL2D)
    Image *rawImg = new Image_GL2D(filePath, fromScratchProject);
#elif defined(RENDERER_HEADLESS)
    Image *rawImg = new Image_Headless(filePath, fromScratchProject);
#else
    throw std::runtime_error("Image backend not defined,");
#endif

    auto img = std::shared_ptr<Image>(rawImg, [filePath](Image *p) {
        images.erase(filePath);
        delete p;
    });

    images[filePath] = img;
    return img;
}

std::shared_ptr<Image> createImageFromZip(std::string filePath, mz_zip_archive *zip) {
    auto it = images.find(filePath);
    if (it != images.end()) {
        if (auto img = it->second.lock()) {
            return img;
        } else {
            images.erase(it);
        }
    }

#if defined(RENDERER_SDL1)
    Image *rawImg = new Image_SDL1(filePath, zip);
#elif defined(RENDERER_SDL2)
    Image *rawImg = new Image_SDL2(filePath, zip);
#elif defined(RENDERER_SDL3)
    Image *rawImg = new Image_SDL3(filePath, zip);
#elif defined(RENDERER_CITRO2D)
    Image *rawImg = new Image_C2D(filePath, zip);
#elif defined(RENDERER_OPENGL)
    Image *rawImg = new Image_GL(filePath, zip);
#elif defined(RENDERER_GL2D)
    Image *rawImg = new Image_GL2D(filePath, zip);
#elif defined(RENDERER_HEADLESS)
    Image *rawImg = new Image_Headless(filePath, zip);
#else
    throw std::runtime_error("Image backend not defined,");
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
    char *svg_data = const_cast<char *>(data);
    NSVGimage *image = nsvgParse(svg_data, "px", 96.0f);
    if (!image) throw std::runtime_error("Failed to parse SVG");

    width = image->width > 0 ? (int)image->width : 32;
    height = image->height > 0 ? (int)image->height : 32;

    NSVGrasterizer *rast = nsvgCreateRasterizer();
    if (!rast) {
        nsvgDelete(image);
        throw std::runtime_error("Failed to create SVG rasterizer");
    }

    unsigned char *pixels = (unsigned char *)malloc(width * height * 4);
    if (!pixels) {
        nsvgDeleteRasterizer(rast);
        nsvgDelete(image);
        throw std::runtime_error("Failed to allocate SVG pixel buffer");
    }

    nsvgRasterize(rast, image, 0, 0, 1.0f, pixels, width, height, width * 4);

    nsvgDeleteRasterizer(rast);
    nsvgDelete(image);

    return pixels;
}

unsigned char *Image::loadRasterFromMemory(const unsigned char *data, size_t size, int &width, int &height) {
    int channels;
    unsigned char *pixels = stbi_load_from_memory(data, size, &width, &height, &channels, 4);
    if (!pixels) throw std::runtime_error("Failed to decode raster image");
    return pixels;
}

Image::Image(std::string filePath, bool fromScratchProject) {

    if (fromScratchProject) {
        if (Unzip::UnpackedInSD) filePath = Unzip::filePath + filePath;
        else filePath = OS::getRomFSLocation() + "project/" + filePath;
    } else filePath = OS::getRomFSLocation() + filePath;

    bool isSVG = filePath.size() >= 4 && (filePath.substr(filePath.size() - 4) == ".svg" || filePath.substr(filePath.size() - 4) == ".SVG");

    std::vector<char> buffer = readFileToBuffer(filePath, fromScratchProject);

    if (isSVG) {
        pixels = loadSVGFromMemory(buffer.data(), buffer.size(), width, height);
    } else {
        pixels = loadRasterFromMemory(reinterpret_cast<unsigned char *>(buffer.data()), buffer.size(), width, height);
    }

    if (!pixels) throw std::runtime_error("Failed to load image: " + filePath);
}

Image::Image(std::string filePath, mz_zip_archive *zip) {
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

        pixels = loadSVGFromMemory(svgBuffer.data(), file_size, width, height);
    } else {
        pixels = loadRasterFromMemory((unsigned char *)file_data, file_size, width, height);
    }

    mz_free(file_data);

    if (!pixels) throw std::runtime_error("Failed to load image from zip: " + filePath);
}

Image::~Image() {
    if (pixels)
        stbi_image_free(pixels);
}

int Image::getWidth() {
    return width;
}

int Image::getHeight() {
    return height;
}