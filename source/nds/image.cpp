#include "image.hpp"
#define STBI_NO_GIF
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
#include "unzip.hpp"
// This image stuff is going to be tough; the DS uses memory "banks" of fixed sizes that might not be the best use of space when it comes to images.
// The vram banks add up to 656 KB.

// There's a bunch of repeated code here from the 3DS. TODO: Look into sharing code between the two.

std::unordered_map<std::string, imagePAL8> images;

const uint16_t next_pow2(uint16_t n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}

const uint16_t clamp(uint16_t n, uint16_t lower, uint16_t upper) {
    if (n < lower)
        return lower;
    if (n > upper)
        return upper;
    return n;
}

Image::Image(std::string filePath) : width(0), height(0), scale(1.0), opacity(1.0), rotation(0.0) {
    if (!loadImageFromFile(filePath, nullptr, false)) return;

    std::string filename = filePath.substr(filePath.find_last_of('/') + 1);
    std::string path2 = filename.substr(0, filename.find_last_of('.'));
    if (images.find(path2) != images.end()) {
        imagePAL8 &img = images[path2];
        imageId = path2;
        width = img.width;
        height = img.height;
        scale = 1.0;
        rotation = 0.0;
        opacity = 1.0;
        Log::log("loaded!");
    }
}

Image::~Image() {
}

void Image::render(double xPos, double yPos, bool centered) {

    auto imgFind = images.find(imageId);
    if (imgFind != images.end()) {
        imagePAL8 &data = images[imageId];
        glImage *image = &images[imageId].image;
        glBindTexture(GL_TEXTURE_2D, data.textureID);

        int RenderX = xPos;
        int RenderY = yPos;

        if (centered) {
            RenderX -= width / 2;
            RenderY -= height / 2;
        }

        int renderScale = (1 << 12);
        // if (data.scaleX != 1 << 12 || data.scaleY != 1 << 12) {
        //     renderScale = (renderScale * data.scaleX) >> 12;
        // }

        glSpriteScale(RenderX, RenderY, renderScale, GL_FLIP_NONE, image);
        data.freeTimer = data.maxFreeTimer;
    }
}

void Image::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
    // we are NOT doing nine-slice rendering on the DS....
    render(xPos, yPos, centered);
}

bool Image::loadImageFromFile(std::string filePath, Sprite *sprite, bool fromScratchProject) {

    std::string filename = filePath.substr(filePath.find_last_of('/') + 1);
    std::string costumeName = filename.substr(0, filename.find_last_of('.'));
    if (images.find(costumeName) != images.end()) return true;

    std::string fullPath = filePath;

    if (fromScratchProject) fullPath = "project/" + fullPath;

    FILE *file = fopen(fullPath.c_str(), "rb");
    if (!file) {
        Log::logWarning("Image file not found: " + fullPath);
        return false;
    }

    imageRGBA newRGBA;
    int width, height;

    bool isSVG = filePath.size() >= 4 &&
                 (filePath.substr(filePath.size() - 4) == ".svg" ||
                  filePath.substr(filePath.size() - 4) == ".SVG");

    if (isSVG) {
        newRGBA.isSVG = true;
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        char *svg_data = (char *)malloc(file_size);
        if (!svg_data) {
            Log::logWarning("Failed to allocate memory for SVG file: " + filePath);
            fclose(file);
            return false;
        }

        size_t read_size = fread(svg_data, 1, file_size, file);
        fclose(file);

        if (read_size != (size_t)file_size) {
            Log::logWarning("Failed to read SVG file completely: " + filePath);
            free(svg_data);
            return false;
        }

        newRGBA.data = SVGToRGBA(svg_data, file_size, width, height);
        free(svg_data);

        if (!newRGBA.data) {
            Log::logWarning("Failed to decode SVG: " + filePath);
            return false;
        }
    } else {
        int channels;
        newRGBA.data =
            stbi_load_from_file(file, &width, &height, &channels, 4);
        fclose(file);

        if (!newRGBA.data) {
            Log::logWarning("Failed to decode image: " + fullPath);
            return false;
        }
    }

    newRGBA.width = width;
    newRGBA.height = height;
    newRGBA.originalWidth = width;
    newRGBA.originalHeight = height;
    newRGBA.scaleX = (newRGBA.isSVG) ? 2 << 12 : 1 << 12;
    newRGBA.scaleY = (newRGBA.isSVG) ? 2 << 12 : 1 << 12;
    newRGBA.textureWidth = clamp(next_pow2(newRGBA.width), 0, 1024);
    newRGBA.textureHeight = clamp(next_pow2(newRGBA.height), 0, 1024);
    newRGBA.textureMemSize = newRGBA.textureWidth * newRGBA.textureHeight * 4;
    if (newRGBA.width > 32 || newRGBA.height > 32) {
        const int largest = std::max(newRGBA.width, newRGBA.height);
        const float scale = 1.0f / std::sqrt((float)largest / 32.0f);
        resizeRGBAImage(newRGBA.width * scale, newRGBA.height * scale, newRGBA);
    }

    imagePAL8 image = RGBAToPAL8(newRGBA);
    if (uploadPAL8ToVRAM(image, &image.image)) {
        images[costumeName] = image;
    }
    stbi_image_free(newRGBA.data);
    return true;
}

void Image::loadImageFromSB3(mz_zip_archive *zip, const std::string &costumeId, Sprite *sprite) {

    std::string filename = costumeId.substr(costumeId.find_last_of('/') + 1);
    std::string costumeName = filename.substr(0, filename.find_last_of('.'));
    if (images.find(costumeName) != images.end()) return;

    // Find the file in the zip
    int file_index = mz_zip_reader_locate_file(zip, costumeId.c_str(), nullptr, 0);
    if (file_index < 0) {
        Log::logWarning("Image file not found in zip: " + costumeId);
        return;
    }

    // Get file stats
    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(zip, file_index, &file_stat)) {
        Log::logWarning("Failed to get file stats for: " + costumeId);
        return;
    }

    bool isSVG = costumeId.size() >= 4 &&
                 (costumeId.substr(costumeId.size() - 4) == ".svg" ||
                  costumeId.substr(costumeId.size() - 4) == ".SVG");

    // Extract file data
    size_t file_size;
    void *file_data = mz_zip_reader_extract_to_heap(zip, file_index, &file_size, 0);
    if (!file_data) {
        Log::logWarning("Failed to extract: " + costumeId);
        return;
    }

    int width, height;

    imageRGBA newRGBA;

    if (isSVG) {
        newRGBA.isSVG = true;
        newRGBA.data = SVGToRGBA(file_data, file_size, width, height);
        if (!newRGBA.data) {
            Log::logWarning("Failed to decode SVG: " + costumeId);
            mz_free(file_data);
            Image::cleanupImages();
            return;
        }
    } else {
        // Handle bitmap files (PNG, JPG)
        int channels;
        newRGBA.data = stbi_load_from_memory(
            (unsigned char *)file_data, file_size,
            &width, &height, &channels, 4);

        if (!newRGBA.data) {
            Log::logWarning("Failed to decode image: " + costumeId);
            mz_free(file_data);
            Image::cleanupImages();
            return;
        }
    }

    // Set up the image data structure
    newRGBA.name = costumeName;
    newRGBA.fullName = costumeId;
    newRGBA.width = width;
    newRGBA.height = height;
    newRGBA.originalWidth = width;
    newRGBA.originalHeight = height;
    newRGBA.scaleX = (newRGBA.isSVG) ? 2 << 12 : 1 << 12;
    newRGBA.scaleY = (newRGBA.isSVG) ? 2 << 12 : 1 << 12;
    newRGBA.textureWidth = clamp(next_pow2(newRGBA.width), 0, 1024);
    newRGBA.textureHeight = clamp(next_pow2(newRGBA.height), 0, 1024);
    newRGBA.textureMemSize = newRGBA.textureWidth * newRGBA.textureHeight * 4;
    mz_free(file_data);

    if (newRGBA.width > 32 || newRGBA.height > 32) {
        const int largest = std::max(newRGBA.width, newRGBA.height);
        const float scale = 1.0f / std::sqrt((float)largest / 32.0f);
        resizeRGBAImage(newRGBA.width * scale, newRGBA.height * scale, newRGBA);
    }

    imagePAL8 image = RGBAToPAL8(newRGBA);
    if (uploadPAL8ToVRAM(image, &image.image)) {
        images[costumeName] = image;
    }
    stbi_image_free(newRGBA.data);
}

bool resizeRGBAImage(uint16_t newWidth, uint16_t newHeight, imageRGBA &rgba) {
    unsigned char *resizedData = new unsigned char[newWidth * newHeight * 4];

    for (int y = 0; y < newHeight; y++) {
        for (int x = 0; x < newWidth; x++) {
            int srcX = x * rgba.width / newWidth;
            int srcY = y * rgba.height / newHeight;

            unsigned char *srcPixel = &rgba.data[(srcY * rgba.width + srcX) * 4];
            unsigned char *dstPixel = &resizedData[(y * newWidth + x) * 4];

            dstPixel[0] = srcPixel[0]; // R
            dstPixel[1] = srcPixel[1]; // G
            dstPixel[2] = srcPixel[2]; // B
            dstPixel[3] = srcPixel[3]; // A
        }
    }

    stbi_image_free(rgba.data);
    rgba.data = resizedData;
    rgba.scaleX = ((rgba.width << 12) / newWidth) * (rgba.isSVG ? 2 : 1);
    rgba.scaleY = ((rgba.height << 12) / newHeight) * (rgba.isSVG ? 2 : 1);
    rgba.width = newWidth;
    rgba.height = newHeight;
    rgba.textureWidth = clamp(next_pow2(rgba.width), 0, 1024);
    rgba.textureHeight = clamp(next_pow2(rgba.height), 0, 1024);
    rgba.textureMemSize = rgba.textureWidth * rgba.textureHeight * 4;
    return true;
}

imagePAL8 RGBAToPAL8(const imageRGBA &rgba) {
    imagePAL8 ds = {};
    ds.width = rgba.width;
    ds.height = rgba.height;
    ds.originalWidth = rgba.originalWidth;
    ds.originalHeight = rgba.originalHeight;
    ds.textureWidth = rgba.textureWidth;
    ds.textureHeight = rgba.textureHeight;
    ds.scaleX = rgba.scaleX;
    ds.scaleY = rgba.scaleY;

    const int imgW = rgba.width;
    const int imgH = rgba.height;
    const int texW = rgba.textureWidth;
    const int texH = rgba.textureHeight;
    const int totalPixels = texW * texH;

    ds.textureData = new unsigned char[totalPixels];

    std::map<unsigned int, int> colorMap;
    std::vector<unsigned short> palette;
    palette.reserve(256);

    // reserve index 0 for transparency
    palette.push_back(0x0000);

    auto findNearestColor = [&](unsigned char r, unsigned char g, unsigned char b) -> int {
        int bestIndex = 1; // Default to index 1 if nothing better found
        int bestDistance = INT_MAX;

        // Start from index 1 (skip transparency at 0)
        for (size_t i = 1; i < palette.size(); ++i) {
            unsigned short rgb555 = palette[i];
            // Extract RGB components from RGB555 format
            int pr = (rgb555 & 0x1F);
            int pg = ((rgb555 >> 5) & 0x1F);
            int pb = ((rgb555 >> 10) & 0x1F);

            // Calculate distance in 5-bit color space
            int r5 = r >> 3;
            int g5 = g >> 3;
            int b5 = b >> 3;

            int dr = r5 - pr;
            int dg = g5 - pg;
            int db = b5 - pb;

            int distance = dr * dr * 2 + dg * dg * 4 + db * db * 3;

            if (distance < bestDistance) {
                bestDistance = distance;
                bestIndex = (int)i;

                if (distance == 0) break;
            }
        }

        return bestIndex;
    };

    for (int y = 0; y < texH; ++y) {
        for (int x = 0; x < texW; ++x) {
            int dstIndex = y * texW + x;

            // If pixel is out of bounds, give it transparency
            if (x >= imgW || y >= imgH) {
                ds.textureData[dstIndex] = 0;
                continue;
            }

            int srcIndex = (y * imgW + x) * 4;
            unsigned char r = rgba.data[srcIndex + 0];
            unsigned char g = rgba.data[srcIndex + 1];
            unsigned char b = rgba.data[srcIndex + 2];
            unsigned char a = rgba.data[srcIndex + 3];

            // get transparency from alpha
            if (a <= 127) {
                ds.textureData[dstIndex] = 0;
                continue;
            }

            unsigned int colorKey = ((unsigned int)r << 16) |
                                    ((unsigned int)g << 8) |
                                    ((unsigned int)b);

            auto it = colorMap.find(colorKey);
            if (it != colorMap.end()) {
                ds.textureData[dstIndex] = (unsigned char)it->second;
            } else {
                if (palette.size() < 256) {
                    int newIndex = (int)palette.size();
                    colorMap[colorKey] = newIndex;
                    unsigned short rgb555 = RGB15(r >> 3, g >> 3, b >> 3) | BIT(15);
                    palette.push_back(rgb555);
                    ds.textureData[dstIndex] = (unsigned char)newIndex;
                } else {
                    // Palette full: find nearest existing color
                    int nearestIndex = findNearestColor(r, g, b);
                    ds.textureData[dstIndex] = (unsigned char)nearestIndex;
                    // Cache this mapping to avoid recalculating
                    colorMap[colorKey] = nearestIndex;
                }
            }
        }
    }

    // Copy palette into DS-format array
    ds.paletteSize = (int)palette.size();
    ds.paletteData = new unsigned short[256];
    for (size_t i = 0; i < palette.size(); ++i)
        ds.paletteData[i] = palette[i];
    for (size_t i = palette.size(); i < 256; ++i)
        ds.paletteData[i] = 0x0000;

    ds.textureMemSize = totalPixels + (256 * 2); // texture bytes + palette bytes
    Log::log("Tex converted! Size: " + std::to_string(ds.textureMemSize / 1000) + " KB");
    return ds;
}

bool uploadPAL8ToVRAM(imagePAL8 &image, glImage *outImage) {
    int texID = glLoadTileSet(
        outImage,
        image.width,
        image.height,
        image.textureWidth,
        image.textureHeight,
        GL_RGB256,
        image.textureWidth,
        image.textureHeight,
        TEXGEN_TEXCOORD | GL_TEXTURE_COLOR0_TRANSPARENT,
        256,
        (const u16 *)image.paletteData,
        (const u8 *)image.textureData);

    freePAL8(image);

    if (texID < 0) {
        Log::logWarning("Failed to upload tex. " +
                        (texID == -1
                             ? std::string(" Out of VRAM!")
                             : " Error " + std::to_string(texID)));

        return false;
    }

    image.textureID = texID;

    return true;
}

/**
 * Loads SVG data and converts it to RGBA pixel data
 */
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

    // Clamp to 3DS limits
    width = clamp(width, 0, 1024);
    height = clamp(height, 0, 1024);

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

    // Calculate scale
    float scale = 1.0f;
    if (image->width > 0 && image->height > 0) {
        float scaleX = (float)width / image->width;
        float scaleY = (float)height / image->height;
        scale = std::min(scaleX, scaleY);
    }

    // Rasterize SVG
    nsvgRasterize(rast, image, 0, 0, scale, rgba_data, width, height, width * 4);

    // Clean up
    nsvgDeleteRasterizer(rast);
    nsvgDelete(image);

    return rgba_data;
}

void freePAL8(imagePAL8 &image) {
    delete[] image.paletteData;
    image.paletteData = nullptr;
    delete[] image.textureData;
    image.textureData = nullptr;
}

void Image::freeImage(const std::string &costumeId) {
    auto imgFind = images.find(costumeId);
    if (imgFind == images.end()) {
        Log::logWarning("Could not find image to free!");
        return;
    }

    imagePAL8 &image = imgFind->second;
    glDeleteTextures(1, &image.textureID);

    images.erase(imgFind);
}

void Image::cleanupImagesLite() {
}

void Image::cleanupImages() {
    std::vector<std::string> toDelete;
    for (auto &[id, image] : images) {
        toDelete.push_back(id);
    }
    for (auto &id : toDelete) {
        freeImage(id);
    }
}

void Image::queueFreeImage(const std::string &costumeId) {
}

void Image::FlushImages() {
    std::vector<std::string> toDelete;
    for (auto &[id, data] : images) {
        data.freeTimer--;
        if (data.freeTimer <= 0) {
            toDelete.push_back(id);
        }
    }
    for (auto &del : toDelete) {
        freeImage(del);
    }
}
