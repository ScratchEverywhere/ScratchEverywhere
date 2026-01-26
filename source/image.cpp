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
#if defined(RENDERER_SDL2)
#include <image_sdl2.hpp>
#endif

std::unordered_map<std::string, std::weak_ptr<Image>> images;

std::shared_ptr<Image> createImageFromFile(std::string filePath, bool fromScratchProject) {
    auto it = images.find(filePath);
    if (it != images.end()) {
        if (auto img = it->second.lock()) {
            return img;
        }
    }
#if defined(RENDERER_SDL2)
    return std::make_shared<Image_SDL2>(filePath, fromScratchProject);
#else
    return std::make_shared<Image>(filePath, fromScratchProject);
#endif
}

std::shared_ptr<Image> createImageFromZip(std::string filePath, mz_zip_archive *zip) {
    return nullptr;
}

Image::Image(std::string filePath, bool fromScratchProject) {

    if (fromScratchProject) filePath = "project/" + filePath;
    if (Unzip::UnpackedInSD) filePath = Unzip::filePath + filePath;

    FILE *file = fopen(filePath.c_str(), "rb");

    if (!file) {
        throw std::runtime_error("Failed to open Texture file: " + filePath);
    }

    bool isSVG = filePath.size() >= 4 && (filePath.substr(filePath.size() - 4) == ".svg" || filePath.substr(filePath.size() - 4) == ".SVG");

    if (isSVG) {
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);

        if (size <= 0) {
            fclose(file);
            throw std::runtime_error("Empty SVG file: " + filePath);
        }

        std::vector<char> svg(size + 1);
        if (fread(svg.data(), 1, size, file) != (size_t)size) {
            fclose(file);
            throw std::runtime_error("Failed to read SVG file: " + filePath);
        }
        svg[size] = '\0';

        NSVGimage *image = nsvgParse(svg.data(), "px", 96.0f);
        if (!image) {
            fclose(file);
            throw std::runtime_error("Failed to parse SVG: " + filePath);
        }

        width = image->width > 0 ? (int)image->width : 32;
        height = image->height > 0 ? (int)image->height : 32;

        // 3DS Clamp
        // width  = std::clamp(width, 0, 1024);
        // height = std::clamp(height, 0, 1024);

        NSVGrasterizer *rast = nsvgCreateRasterizer();
        if (!rast) {
            nsvgDelete(image);
            fclose(file);
            throw std::runtime_error("Failed to create SVG rasterizer");
        }

        pixels = (unsigned char *)malloc(width * height * 4);
        if (!pixels) {
            nsvgDeleteRasterizer(rast);
            nsvgDelete(image);
            fclose(file);
            throw std::runtime_error("Failed to allocate SVG pixel buffer");
        }

        nsvgRasterize(rast, image, 0, 0, 1.0f, (unsigned char *)pixels, width, height, width * 4);

        nsvgDeleteRasterizer(rast);
        nsvgDelete(image);
    } else {
        int channels;
        pixels = stbi_load_from_file(file, &width, &height, &channels, 4);
    }
    fclose(file);

    if (!pixels) {
        throw std::runtime_error("Failed to decode Texture: " + filePath);
    }
}

Image::Image(std::string filePath, mz_zip_archive *zip) {
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

std::string Image::getID() {
    return id;
}