#include "image.hpp"
#include "render.hpp"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <math.hpp>
#include <os.hpp>
#include <string>
#include <unordered_map>
#include <unzip.hpp>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

std::unordered_map<std::string, ImageData> images;
static std::vector<std::string> toDelete;

#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>
CMRC_DECLARE(romfs);
#endif

unsigned char *SVGToRGBA(const void *svg_data, size_t svg_size, int &width, int &height) {
    // Create a null-terminated string from the SVG data
    char *svg_string = (char *)malloc(svg_size + 1);
    if (!svg_string) {
        Log::logWarning("Failed to allocate memory for SVG string");
        return nullptr;
    }
    memcpy(svg_string, svg_data, svg_size);
    svg_string[svg_size] = '\0';

    // Parse SVG
    NSVGimage *image = nsvgParse(svg_string, "px", 96.0f);
    free(svg_string);

    if (!image) {
        Log::logWarning("Failed to parse SVG");
        return nullptr;
    }

    // Determine render size
    if (image->width > 0 && image->height > 0) {
        width = (int)image->width;
        height = (int)image->height;
    } else {
        width = 32;
        height = 32;
    }

    // Create rasterizer
    NSVGrasterizer *rast = nsvgCreateRasterizer();
    if (!rast) {
        Log::logWarning("Failed to create SVG rasterizer");
        nsvgDelete(image);
        return nullptr;
    }

    // Allocate RGBA buffer
    unsigned char *rgba_data = (unsigned char *)malloc(width * height * 4);
    if (!rgba_data) {
        Log::logWarning("Failed to allocate RGBA buffer for SVG");
        nsvgDeleteRasterizer(rast);
        nsvgDelete(image);
        return nullptr;
    }

    // Rasterize SVG
    nsvgRasterize(rast, image, 0, 0, 1.0f, rgba_data, width, height, width * 4);

    // Clean up
    nsvgDeleteRasterizer(rast);
    nsvgDelete(image);

    return rgba_data;
}

Image::Image(std::string filePath) {
    if (!loadImageFromFile(filePath, nullptr, false)) return;

    std::string filename = filePath.substr(filePath.find_last_of('/') + 1);
    std::string path2 = filename.substr(0, filename.find_last_of('.'));
    ImageData &image = images[path2];

    imageId = path2;
    width = image.width;
    height = image.height;
    scale = 1.0;
    rotation = 0.0;
    opacity = 1.0;
    images[imageId].imageUsageCount++;
}

Image::~Image() {
    auto it = images.find(imageId);
    if (it != images.end()) {
        images[imageId].imageUsageCount--;
        if (images[imageId].imageUsageCount <= 0)
            freeImage(imageId);
    }
}

void Image::render(double xPos, double yPos, bool centered) {
    if (images.find(imageId) == images.end()) return;
    ImageData &image = images[imageId];

    image.freeTimer = image.maxFreeTime;

    glBindTexture(GL_TEXTURE_2D, image.textureID);
    glColor4f(1.0f, 1.0f, 1.0f, opacity);

    glPushMatrix();
    glTranslatef(xPos, yPos, 0.0f);
    glRotatef(rotation, 0.0f, 0.0f, 1.0f);
    glScalef(scale, scale, 1.0f);
    if (centered) {
        glTranslatef(-width / 2.0f, -height / 2.0f, 0.0f);
    }

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(width, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(width, height);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(0.0f, height);
    glEnd();

    glPopMatrix();
}

void Image::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
    if (images.find(imageId) == images.end()) return;
    ImageData &image = images[imageId];

    image.freeTimer = image.maxFreeTime;

    glBindTexture(GL_TEXTURE_2D, image.textureID);
    glColor4f(1.0f, 1.0f, 1.0f, opacity);

    float destX = (float)xPos;
    float destY = (float)yPos;

    if (centered) {
        destX -= (float)width / 2.0f;
        destY -= (float)height / 2.0f;
    }

    float imgW = (float)image.width;
    float imgH = (float)image.height;
    float p = (float)padding;
    float w = (float)width;
    float h = (float)height;

    glBegin(GL_QUADS);

    auto drawSubRect = [&](float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh) {
        float u0 = sx / imgW;
        float v0 = sy / imgH;
        float u1 = (sx + sw) / imgW;
        float v1 = (sy + sh) / imgH;
        glTexCoord2f(u0, v0);
        glVertex2f(dx, dy);
        glTexCoord2f(u1, v0);
        glVertex2f(dx + dw, dy);
        glTexCoord2f(u1, v1);
        glVertex2f(dx + dw, dy + dh);
        glTexCoord2f(u0, v1);
        glVertex2f(dx, dy + dh);
    };

    // Top-left
    drawSubRect(0, 0, p, p, destX, destY, p, p);
    // Top
    drawSubRect(p, 0, imgW - p * 2, p, destX + p, destY, w - p * 2, p);
    // Top-right
    drawSubRect(imgW - p, 0, p, p, destX + w - p, destY, p, p);

    // Left
    drawSubRect(0, p, p, imgH - p * 2, destX, destY + p, p, h - p * 2);
    // Center
    drawSubRect(p, p, imgW - p * 2, imgH - p * 2, destX + p, destY + p, w - p * 2, h - p * 2);
    // Right
    drawSubRect(imgW - p, p, p, imgH - p * 2, destX + w - p, destY + p, p, h - p * 2);

    // Bottom-left
    drawSubRect(0, imgH - p, p, p, destX, destY + h - p, p, p);
    // Bottom
    drawSubRect(p, imgH - p, imgW - p * 2, p, destX + p, destY + h - p, w - p * 2, p);
    // Bottom-right
    drawSubRect(imgW - p, imgH - p, p, p, destX + w - p, destY + h - p, p, p);

    glEnd();
}

bool get_GL_Image(imageRGBA &rgba) {
    ImageData image;
    image.width = rgba.width;
    image.height = rgba.height;
    image.isSVG = rgba.isSVG;

    glGenTextures(1, &image.textureID);
    glBindTexture(GL_TEXTURE_2D, image.textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba.width, rgba.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data);

    images[rgba.name] = image;
    return true;
}

bool Image::loadImageFromFile(std::string filePath, Sprite *sprite, bool fromScratchProject) {
    std::string filename = filePath.substr(filePath.find_last_of('/') + 1);
    std::string path2 = filename.substr(0, filename.find_last_of('.'));

    if (images.find(path2) != images.end()) return true;

    std::string fullPath;
    fullPath = OS::getRomFSLocation();
    if (fromScratchProject)
        fullPath += "project/";
    fullPath += filePath;

    if (Unzip::UnpackedInSD && fromScratchProject) fullPath = Unzip::filePath + filePath;

    int width, height, channels;
    unsigned char *rgba_data = nullptr;

    bool isSVG = filePath.size() >= 4 &&
                 (filePath.substr(filePath.size() - 4) == ".svg" ||
                  filePath.substr(filePath.size() - 4) == ".SVG");

    imageRGBA newRGBA;

#ifdef USE_CMAKERC
    if (!Unzip::UnpackedInSD || !fromScratchProject) {
        try {
            const auto &fs = cmrc::romfs::get_filesystem();
            if (fs.exists(fullPath)) {
                const auto &file = fs.open(fullPath);
                if (isSVG) {
                    rgba_data = SVGToRGBA(file.begin(), file.size(), width, height);
                } else {
                    rgba_data = stbi_load_from_memory((const stbi_uc *)file.begin(), file.size(), &width, &height, &channels, 4);
                }
            }
        } catch (...) {
        }
    }
#endif

    if (rgba_data == nullptr) {
        FILE *file = fopen(fullPath.c_str(), "rb");
        if (file) {
            if (isSVG) {
                fseek(file, 0, SEEK_END);
                long size = ftell(file);
                fseek(file, 0, SEEK_SET);
                char *svg_data = (char *)malloc(size + 1);
                fread(svg_data, 1, size, file);
                fclose(file);
                svg_data[size] = '\0';
                rgba_data = SVGToRGBA(svg_data, size, width, height);
                free(svg_data);
            } else {
                rgba_data = stbi_load_from_file(file, &width, &height, &channels, 4);
                fclose(file);
            }
        }
    }

    if (rgba_data) {
        newRGBA.name = path2;
        newRGBA.fullName = filename;
        newRGBA.width = width;
        newRGBA.height = height;
        newRGBA.isSVG = isSVG;
        newRGBA.data = rgba_data;

        if (sprite != nullptr) {
            sprite->spriteWidth = width / 2;
            sprite->spriteHeight = height / 2;
        }

        bool success = get_GL_Image(newRGBA);
        if (isSVG) free(rgba_data);
        else stbi_image_free(rgba_data);
        return success;
    }

    Log::logWarning("Failed to load image: " + fullPath);
    return false;
}

void Image::loadImageFromSB3(mz_zip_archive *zip, const std::string &costumeId, Sprite *sprite) {
    std::string imgId = costumeId.substr(0, costumeId.find_last_of('.'));
    if (images.find(imgId) != images.end()) return;

    int file_index = mz_zip_reader_locate_file(zip, costumeId.c_str(), nullptr, 0);
    if (file_index < 0) return;

    size_t file_size;
    void *file_data = mz_zip_reader_extract_to_heap(zip, file_index, &file_size, 0);
    if (!file_data) return;

    int width, height, channels;
    unsigned char *rgba_data = nullptr;
    bool isSVG = costumeId.size() >= 4 && (costumeId.substr(costumeId.size() - 4) == ".svg" || costumeId.substr(costumeId.size() - 4) == ".SVG");

    if (isSVG) {
        rgba_data = SVGToRGBA(file_data, file_size, width, height);
    } else {
        rgba_data = stbi_load_from_memory((unsigned char *)file_data, file_size, &width, &height, &channels, 4);
    }
    mz_free(file_data);

    if (rgba_data) {
        imageRGBA newRGBA;
        newRGBA.name = imgId;
        newRGBA.fullName = costumeId;
        newRGBA.width = width;
        newRGBA.height = height;
        newRGBA.isSVG = isSVG;
        newRGBA.data = rgba_data;

        if (sprite != nullptr) {
            sprite->spriteWidth = width / 2;
            sprite->spriteHeight = height / 2;
        }

        get_GL_Image(newRGBA);
        if (isSVG) free(rgba_data);
        else stbi_image_free(rgba_data);
    }
}

void Image::cleanupImagesLite() {
    std::vector<std::string> keysToDelete;
    for (const auto &[id, data] : images) {
        if (data.freeTimer < data.maxFreeTime - 2)
            keysToDelete.push_back(id);
    }

    for (const std::string &id : keysToDelete) {
        Image::freeImage(id);
    }
}

void Image::cleanupImages() {
    std::vector<std::string> keysToDelete;
    for (auto &[id, data] : images) {
        keysToDelete.push_back(id);
    }
    for (const std::string &id : keysToDelete) {
        Image::freeImage(id);
    }
    images.clear();
}

void Image::freeImage(const std::string &costumeId) {
    auto it = images.find(costumeId);
    if (it != images.end()) {
        glDeleteTextures(1, &it->second.textureID);
        images.erase(it);
    }
}

void Image::FlushImages() {
    std::vector<std::string> keysToDelete;
    for (auto &[id, data] : images) {
        data.freeTimer--;
        if (data.freeTimer <= 0) {
            keysToDelete.push_back(id);
        }
    }

    for (const std::string &id : keysToDelete) {
        freeImage(id);
    }

    for (const std::string &id : toDelete) {
        freeImage(id);
    }
    toDelete.clear();
}

void Image::queueFreeImage(const std::string &costumeId) {
    toDelete.push_back(costumeId);
}
