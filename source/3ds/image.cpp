#include "image.hpp"
#include "../scratch/os.hpp"
#include <algorithm>
#include <iostream>
#include <vector>
#define STBI_NO_GIF
#define STB_IMAGE_IMPLEMENTATION
#include "../scratch/unzip.hpp"
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
#include "stb_image.h"

using u32 = uint32_t;
using u8 = uint8_t;

std::unordered_map<std::string, ImageData> imageC2Ds;
std::vector<Image::ImageRGBA> Image::imageRGBAS;
static std::vector<Image::ImageRGBA *> imageLoadQueue;
static std::vector<std::string> toDelete;
#define MAX_IMAGE_VRAM 30000000

struct MemoryStats {
    size_t totalRamUsage = 0;
    size_t totalVRamUsage = 0;
    size_t imageCount = 0;
    size_t c2dImageCount = 0;
};

static MemoryStats memStats;

const u32 next_pow2(u32 n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}
const u32 clamp(u32 n, u32 lower, u32 upper) {
    if (n < lower)
        return lower;
    if (n > upper)
        return upper;
    return n;
}
const u32 rgba_to_abgr(u32 px) {
    u8 r = (px & 0xff000000) >> 24;
    u8 g = (px & 0x00ff0000) >> 16;
    u8 b = (px & 0x0000ff00) >> 8;
    u8 a = px & 0x000000ff;
    return (a << 24) | (b << 16) | (g << 8) | r;
}

/**
 * Takes every Image from the Scratch's sb3 file and converts them to RGBA data
 */
void Image::loadImages(mz_zip_archive *zip) {
    // Loop through all files in the ZIP
    Log::log("Loading images...");

    int file_count = (int)mz_zip_reader_get_num_files(zip);

    for (int i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip, i, &file_stat)) continue;

        std::string zipFileName = file_stat.m_filename;

        // Check if file is bitmap, or SVG
        bool isBitmap = zipFileName.size() >= 4 &&
                        (zipFileName.substr(zipFileName.size() - 4) == ".png" ||
                         zipFileName.substr(zipFileName.size() - 4) == ".PNG" ||
                         zipFileName.substr(zipFileName.size() - 4) == ".jpg" ||
                         zipFileName.substr(zipFileName.size() - 4) == ".JPG");
        bool isSVG = zipFileName.size() >= 4 &&
                     (zipFileName.substr(zipFileName.size() - 4) == ".svg" ||
                      zipFileName.substr(zipFileName.size() - 4) == ".SVG");

        if (isBitmap || isSVG) {
            size_t file_size;
            void *file_data = mz_zip_reader_extract_to_heap(zip, i, &file_size, 0);
            if (!file_data) {
                printf("Failed to extract %s\n", zipFileName.c_str());
                continue;
            }

            int width, height;
            unsigned char *rgba_data = nullptr;

            Image::ImageRGBA newRGBA;

            if (isSVG) {
                newRGBA.isSVG = true;
                rgba_data = SVGToRGBA(file_data, file_size, width, height);
                if (!rgba_data) {
                    printf("Failed to decode SVG: %s\n", zipFileName.c_str());
                    mz_free(file_data);
                    continue;
                }
            } else {
                // bitmap files
                int channels;
                rgba_data = stbi_load_from_memory(
                    (unsigned char *)file_data, file_size,
                    &width, &height, &channels, 4);

                if (!rgba_data) {
                    printf("Failed to decode image: %s\n", zipFileName.c_str());
                    mz_free(file_data);
                    return; // blablabla running out of memory blablabla
                }
            }

            newRGBA.name = zipFileName.substr(0, zipFileName.find_last_of('.'));
            newRGBA.width = width;
            newRGBA.height = height;
            newRGBA.textureWidth = clamp(next_pow2(newRGBA.width), 64, 1024);
            newRGBA.textureHeight = clamp(next_pow2(newRGBA.height), 64, 1024);
            newRGBA.textureMemSize = newRGBA.textureWidth * newRGBA.textureHeight * 4;
            newRGBA.data = rgba_data;

            size_t imageSize = width * height * 4;
            MemoryTracker::allocate(imageSize);

            Image::imageRGBAS.push_back(newRGBA);
            mz_free(file_data);
        }
    }
}

/**
 * Turns a single image from an unzipped Scratch project into RGBA data
 */
void Image::loadImageFromFile(std::string filePath) {
    std::string filename = filePath.substr(filePath.find_last_of('/') + 1);
    std::string path2 = filename.substr(0, filename.find_last_of('.'));

    auto it = std::find_if(imageRGBAS.begin(), imageRGBAS.end(), [&](const ImageRGBA &img) {
        return img.name == path2;
    });
    if (it != imageRGBAS.end()) return;

    std::string fullPath = "romfs:/project/" + filePath;
    FILE *file = fopen(fullPath.c_str(), "rb");
    if (!file) {
        Log::logWarning("Invalid image file name " + filePath);
        return;
    }

    int width, height;
    unsigned char *rgba_data = nullptr;

    // Check if it's an SVG file
    bool isSVG = filePath.size() >= 4 &&
                 (filePath.substr(filePath.size() - 4) == ".svg" ||
                  filePath.substr(filePath.size() - 4) == ".SVG");

    ImageRGBA newRGBA;

    if (isSVG) {
        // Read entire SVG file into memory
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        char *svg_data = (char *)malloc(file_size);
        if (!svg_data) {
            Log::logWarning("Failed to allocate memory for SVG file: " + filePath);
            fclose(file);
            return;
        }

        size_t read_size = fread(svg_data, 1, file_size, file);
        fclose(file);

        if (read_size != (size_t)file_size) {
            Log::logWarning("Failed to read SVG file completely: " + filePath);
            free(svg_data);
            return;
        }
        newRGBA.isSVG = true;
        rgba_data = SVGToRGBA(svg_data, file_size, width, height);
        free(svg_data);

        if (!rgba_data) {
            Log::logWarning("Failed to decode SVG: " + filePath);
            return;
        }
    } else {
        // Handle regular image files (PNG, JPG)
        int channels;
        rgba_data = stbi_load_from_file(file, &width, &height, &channels, 4);
        fclose(file);

        if (!rgba_data) {
            Log::logWarning("Failed to decode image: " + filePath);
            return;
        }
    }

    newRGBA.name = path2;
    newRGBA.width = width;
    newRGBA.height = height;
    newRGBA.textureWidth = clamp(next_pow2(newRGBA.width), 64, 1024);
    newRGBA.textureHeight = clamp(next_pow2(newRGBA.height), 64, 1024);
    newRGBA.textureMemSize = newRGBA.textureWidth * newRGBA.textureHeight * 4;
    newRGBA.data = rgba_data;

    size_t imageSize = width * height * 4;
    MemoryTracker::allocate(imageSize);

    Log::log("successfuly laoded image from file!");
    imageRGBAS.push_back(newRGBA);
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

    std::cout << width << " " << height << std::endl;

    // Clamp to 3DS limits
    width = clamp(width, 64, 1024);
    height = clamp(height, 64, 1024);

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

/**
 * Queues RGBA image data to be loaded into a Citro2D Image. Image will wait to load if VRAM is too high.
 * @param rgba
 */
bool queueC2DImage(Image::ImageRGBA &rgba) {
    bool inQueue = false;
    for (Image::ImageRGBA *queueRgba : imageLoadQueue) {
        if (rgba.name == queueRgba->name) {
            inQueue = true;
        }
    }
    if (!inQueue) {
        // no queue!!!
        if (memStats.totalVRamUsage + rgba.textureMemSize < MAX_IMAGE_VRAM) {
            return get_C2D_Image(rgba);
        }
        // add to queue D:
        else {
            Log::logWarning("Memory too high! queueing image load!");
            imageLoadQueue.push_back(&rgba);
        }
    }
    return false;
}

/**
 * Reads an `Image::ImageRGBA` image, and adds a `C2D_Image` object to `imageC2Ds`.
 * Assumes image data is stored left->right, top->bottom.
 * Dimensions must be within 64x64 and 1024x1024.
 * Code here originally from https://gbatemp.net/threads/citro2d-c2d_image-example.668574/
 * then edited to fit my code
 */
bool get_C2D_Image(Image::ImageRGBA rgba) {

    // u32 px_count = rgba.width * rgba.height;
    u32 *rgba_raw = reinterpret_cast<u32 *>(rgba.data);

    // Image data
    C2D_Image image;

    // Base texture
    C3D_Tex *tex = MemoryTracker::allocate<C3D_Tex>();
    new (tex) C3D_Tex();
    image.tex = tex;

    // Texture dimensions must be square powers of two between 64x64 and 1024x1024
    tex->width = rgba.textureWidth;
    tex->height = rgba.textureHeight;

    size_t textureSize = rgba.textureMemSize;
    memStats.totalVRamUsage += textureSize;

    // Subtexture
    Tex3DS_SubTexture *subtex = MemoryTracker::allocate<Tex3DS_SubTexture>();
    new (subtex) Tex3DS_SubTexture();

    image.subtex = subtex;
    subtex->width = rgba.width;
    subtex->height = rgba.height;

    // (U, V) coordinates
    subtex->left = 0.0f;
    subtex->top = 1.0f;
    subtex->right = (float)rgba.width / (float)tex->width;
    subtex->bottom = 1.0 - ((float)rgba.height / (float)tex->height);

    if (!C3D_TexInit(tex, tex->width, tex->height, GPU_RGBA8)) {
        Log::logWarning("Texture initializing failed!");
        MemoryTracker::deallocate(tex);
        MemoryTracker::deallocate(subtex);
        return false;
    }
    C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST);

    if (!tex->data) {
        Log::logWarning("Texture data is null!");
        C3D_TexDelete(tex);
        MemoryTracker::deallocate(tex);
        MemoryTracker::deallocate(subtex);
        return false;
    }

    memset(tex->data, 0, textureSize);
    for (u32 i = 0; i < (u32)rgba.width; i++) {
        for (u32 j = 0; j < (u32)rgba.height; j++) {
            u32 src_idx = (j * rgba.width) + i;
            u32 rgba_px = rgba_raw[src_idx];
            u32 abgr_px = rgba_to_abgr(rgba_px);

            // Swizzle magic to convert into a t3x format
            u32 dst_ptr_offset = ((((j >> 3) * (tex->width >> 3) + (i >> 3)) << 6) +
                                  ((i & 1) | ((j & 1) << 1) | ((i & 2) << 1) |
                                   ((j & 2) << 2) | ((i & 4) << 2) | ((j & 4) << 3)));
            ((u32 *)tex->data)[dst_ptr_offset] = abgr_px;
        }
    }

    Log::log("C2D Image Successfully loaded!");

    imageC2Ds[rgba.name] = {image, 120};
    C3D_FrameSync(); // wait for Async functions to finish
    return true;
}

/**
 * Frees a `C2D_Image` from memory using `costumeId` string to find it.
 */
void Image::freeImage(const std::string &costumeId) {
    auto it = imageC2Ds.find(costumeId);
    if (it != imageC2Ds.end()) {
        Log::log("freed image " + it->first);
        if (it->second.image.tex) {

            size_t textureSize = it->second.image.tex->width * it->second.image.tex->height * 4;
            memStats.totalVRamUsage -= textureSize;
            memStats.c2dImageCount--;

            C3D_TexDelete(it->second.image.tex);
            MemoryTracker::deallocate<C3D_Tex>(it->second.image.tex);
        }
        if (it->second.image.subtex) {
            MemoryTracker::deallocate<Tex3DS_SubTexture>((Tex3DS_SubTexture *)it->second.image.subtex);
        }
        imageC2Ds.erase(it);
    }
    if (projectType == UNZIPPED) {
        freeRGBA(costumeId);
    }
}

void freeRGBA(const std::string &imageName) {
    auto it = std::find_if(Image::imageRGBAS.begin(), Image::imageRGBAS.end(), [&](const Image::ImageRGBA &img) {
        return img.name == imageName;
    });

    if (it != Image::imageRGBAS.end()) {
        size_t dataSize = it->width * it->height * 4;
        if (it->data && dataSize > 0) {
            stbi_image_free(it->data);
            MemoryTracker::deallocate(nullptr, dataSize);
            memStats.totalRamUsage -= dataSize;
            memStats.imageCount--;

            Log::log("Freed RGBA data for " + imageName);
        }
        Image::imageRGBAS.erase(it);
    }
}

/**
 * Queues a `C2D_Image` to be freed using `costumeId` to find it.
 * The image will be freed once `FlushImages()` is called.
 */
void Image::queueFreeImage(const std::string &costumeId) {
    toDelete.push_back(costumeId);
}

/**
 * Checks every `C2D_Image` in memory to see if they can be freed.
 * A `C2D_Image` will get freed if there's either too many images in memory,
 * or if a `C2D_Image` goes unused for 120 frames.
 */
void Image::FlushImages() {

    // free unused images if vram usage is high
    if (memStats.totalVRamUsage > MAX_IMAGE_VRAM * 0.8) {
        ImageData *imgToDelete = nullptr;
        std::string toDeleteStr;
        for (auto &[id, img] : imageC2Ds) {
            if (imgToDelete == nullptr) imgToDelete = &img;

            if (img.freeTimer < imgToDelete->freeTimer) {
                imgToDelete = &img;
                toDeleteStr = id;
            }
        }
        toDelete.push_back(toDeleteStr);
    }
    // free images on a timer
    for (auto &[id, img] : imageC2Ds) {
        if (img.freeTimer <= 0) {
            toDelete.push_back(id);
        } else {
            img.freeTimer -= 1;
        }
    }

    for (const std::string &id : toDelete) {
        Image::freeImage(id);
    }
    toDelete.clear();

    if (!imageLoadQueue.empty()) {
        ImageRGBA *rgba = imageLoadQueue.front();
        if (memStats.totalVRamUsage + rgba->textureMemSize < MAX_IMAGE_VRAM) {
            get_C2D_Image(*rgba);
            imageLoadQueue.erase(imageLoadQueue.begin());
        }
    }
}