#include "image.hpp"
#include "os.hpp"
#include <algorithm>
#include <string>
#include <vector>
#define STBI_NO_GIF
#define STB_IMAGE_IMPLEMENTATION
#include "unzip.hpp"
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
#include "stb_image.h"

using u32 = uint32_t;
using u8 = uint8_t;

std::unordered_map<std::string, ImageData> imageC2Ds;
std::vector<imageRGBA> imageRGBAS;
static std::vector<imageRGBA *> imageLoadQueue;
static std::vector<std::string> toDelete;
#define MAX_IMAGE_VRAM 30000000

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

Image::Image(std::string filePath) {
    if (!loadImageFromFile(filePath, false)) return;

    // Find the matching RGBA data in the vector
    std::string filename = filePath.substr(filePath.find_last_of('/') + 1);
    std::string path2 = filename.substr(0, filename.find_last_of('.'));
    for (const auto &rgba : imageRGBAS) {
        if (rgba.name == path2) {
            imageId = rgba.name;
            width = rgba.width;
            height = rgba.height;
            scale = 1.0;
            rotation = 0.0;
            opacity = 1.0;
            if (imageC2Ds.find(rgba.name) == imageC2Ds.end())
                get_C2D_Image(rgba);
            ++imageC2Ds[rgba.name].imageUsageCount;
            return;
        }
    }
}

Image::~Image() {
    if (imageC2Ds.find(imageId) != imageC2Ds.end()) {
        imageC2Ds[imageId].imageUsageCount--;
        if (imageC2Ds[imageId].imageUsageCount <= 0)
            freeImage(imageId);
    }
}

void Image::render(double xPos, double yPos, bool centered) {
    auto rgbaIt = std::find_if(imageRGBAS.begin(), imageRGBAS.end(), [&](const imageRGBA &img) {
        return img.name == imageId;
    });
    if (rgbaIt == imageRGBAS.end() || imageC2Ds.find(rgbaIt->name) == imageC2Ds.end()) return;

    imageC2Ds[rgbaIt->name].freeTimer = imageC2Ds[rgbaIt->name].maxFreeTimer;
    C2D_ImageTint tinty;
    C2D_AlphaImageTint(&tinty, opacity);

    double renderPositionX = xPos;
    double renderPositionY = yPos;

    if (!centered) {
        renderPositionX += getWidth() / 2;
        renderPositionY += getHeight() / 2;
    }

    C2D_DrawImageAtRotated(imageC2Ds[rgbaIt->name].image, static_cast<int>(renderPositionX), static_cast<int>(renderPositionY), 1, rotation, &tinty, scale, scale);
}

void renderSubrect(C2D_Image img, uint16_t srcX, uint16_t srcY, uint16_t srcW, uint16_t srcH, float destX, float destY, float destW, float destH, C2D_ImageTint *tint) {
    uint16_t texW = img.subtex->width - 1;
    texW |= texW >> 1;
    texW |= texW >> 2;
    texW |= texW >> 4;
    texW |= texW >> 8;
    texW++;

    uint16_t texH = img.subtex->height - 1;
    texH |= texH >> 1;
    texH |= texH >> 2;
    texH |= texH >> 4;
    texH |= texH >> 8;
    texH++;

    const Tex3DS_SubTexture subtex = {
        srcW,
        srcH,
        static_cast<float>(srcX) / texW,
        1 - static_cast<float>(srcY) / texH,
        static_cast<float>(srcX + srcW) / texW,
        1 - static_cast<float>(srcY + srcH) / texH};

    C2D_DrawImageAt({img.tex, &subtex}, destX, destY, 1, tint, 1.0f / srcW * destW, 1.0f / srcH * destH);
}

void Image::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
    auto rgbaIt = std::find_if(imageRGBAS.begin(), imageRGBAS.end(), [&](const imageRGBA &img) {
        return img.name == imageId;
    });
    if (rgbaIt == imageRGBAS.end() || imageC2Ds.find(rgbaIt->name) == imageC2Ds.end()) return;

    imageC2Ds[rgbaIt->name].freeTimer = imageC2Ds[rgbaIt->name].maxFreeTimer;
    C2D_ImageTint tinty;
    C2D_AlphaImageTint(&tinty, opacity);

    int renderPositionX = xPos;
    int renderPositionY = yPos;

    if (centered) {
        renderPositionX -= width / 2;
        renderPositionY -= height / 2;
    }

    // To anyone who needs to edit this, I hope you have an ultra-wide monitor
    renderSubrect(imageC2Ds[rgbaIt->name].image, 0, 0, padding, padding, renderPositionX, renderPositionY, padding, padding, &tinty);                                                                                               // Top Left
    renderSubrect(imageC2Ds[rgbaIt->name].image, padding, 0, this->width - padding * 2, padding, renderPositionX + padding, renderPositionY, width - padding * 2, padding, &tinty);                                                 // Top
    renderSubrect(imageC2Ds[rgbaIt->name].image, this->width - padding, 0, padding, padding, renderPositionX + width - padding, renderPositionY, padding, padding, &tinty);                                                         // Top Right
    renderSubrect(imageC2Ds[rgbaIt->name].image, 0, padding, padding, this->height - padding * 2, renderPositionX, renderPositionY + padding, padding, height - padding * 2, &tinty);                                               // Left
    renderSubrect(imageC2Ds[rgbaIt->name].image, padding, padding, this->width - padding * 2, this->height - padding * 2, renderPositionX + padding, renderPositionY + padding, width - padding * 2, height - padding * 2, &tinty); // Center
    renderSubrect(imageC2Ds[rgbaIt->name].image, this->width - padding, padding, padding, this->height - padding * 2, renderPositionX + width - padding, renderPositionY + padding, padding, height - padding * 2, &tinty);         // Right
    renderSubrect(imageC2Ds[rgbaIt->name].image, 0, this->height - padding, padding, padding, renderPositionX, renderPositionY + height - padding, padding, padding, &tinty);                                                       // Bottom Left
    renderSubrect(imageC2Ds[rgbaIt->name].image, padding, this->height - padding, this->width - padding * 2, padding, renderPositionX + padding, renderPositionY + height - padding, width - padding * 2, padding, &tinty);         // Bottom
    renderSubrect(imageC2Ds[rgbaIt->name].image, this->width - padding, this->height - padding, padding, padding, renderPositionX + width - padding, renderPositionY + height - padding, padding, padding, &tinty);                 // Bottom Right
}

/**
 * Turns a single image from an unzipped Scratch project into RGBA data
 */
bool Image::loadImageFromFile(std::string filePath, bool fromScratchProject) {
    std::string filename = filePath.substr(filePath.find_last_of('/') + 1);
    std::string path2 = filename.substr(0, filename.find_last_of('.'));

    auto it = std::find_if(imageRGBAS.begin(), imageRGBAS.end(), [&](const imageRGBA &img) {
        return img.name == path2;
    });
    if (it != imageRGBAS.end()) return true;
    if (getImageFromT3x("romfs:/gfx/" + path2 + ".t3x")) return true;

    std::string fullPath;
    if (fromScratchProject) fullPath = "romfs:/project/" + filePath;
    else fullPath = "romfs:/" + filePath;
    if (Unzip::UnpackedInSD) fullPath = Unzip::filePath + filePath;
    FILE *file = fopen(fullPath.c_str(), "rb");
    if (!file) {
        Log::logWarning("Image file not found: " + fullPath);
        return false;
    }

    int width, height;
    unsigned char *rgba_data = nullptr;

    bool isSVG = filePath.size() >= 4 &&
                 (filePath.substr(filePath.size() - 4) == ".svg" ||
                  filePath.substr(filePath.size() - 4) == ".SVG");

    imageRGBA newRGBA;

    if (isSVG) {
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

        newRGBA.isSVG = true;
        rgba_data = SVGToRGBA(svg_data, file_size, width, height);
        free(svg_data);

        if (!rgba_data) {
            Log::logWarning("Failed to decode SVG: " + filePath);
            return false;
        }
    } else {
        int channels;
        rgba_data = stbi_load_from_file(file, &width, &height, &channels, 4);
        fclose(file);

        if (!rgba_data) {
            Log::logWarning("Failed to decode image: " + filePath);
            return false;
        }
    }

    newRGBA.name = path2;
    newRGBA.fullName = filename;
    newRGBA.width = width;
    newRGBA.height = height;
    newRGBA.textureWidth = clamp(Math::next_pow2(newRGBA.width), 64, 1024);
    newRGBA.textureHeight = clamp(Math::next_pow2(newRGBA.height), 64, 1024);
    newRGBA.textureMemSize = newRGBA.textureWidth * newRGBA.textureHeight * 4;
    newRGBA.data = rgba_data;

    size_t imageSize = width * height * 4;
    MemoryTracker::allocate(imageSize);

    // Log::log("successfuly laoded image from file!");
    imageRGBAS.push_back(newRGBA);
    return true;
}

/**
 * Loads a single image from a Scratch sb3 zip file by filename.
 * @param zip Pointer to the zip archive
 * @param costumeId The filename of the image to load (e.g., "sprite1.png")
 */
void Image::loadImageFromSB3(mz_zip_archive *zip, const std::string &costumeId) {
    std::string imageId = costumeId.substr(0, costumeId.find_last_of('.'));

    // Check if image already exists
    auto it = std::find_if(imageRGBAS.begin(), imageRGBAS.end(), [&](const imageRGBA &img) {
        return img.name == imageId;
    });
    if (it != imageRGBAS.end()) return;

    // Log::log("Loading single image: " + costumeId);

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

    // Check if file is bitmap or SVG
    bool isBitmap = costumeId.size() > 4 && ([](std::string ext) {
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" ||
                               ext == ".bmp" || ext == ".psd" || ext == ".gif" || ext == ".hdr" ||
                               ext == ".pic" || ext == ".ppm" || ext == ".pgm";
                    }(costumeId.substr(costumeId.find_last_of('.'))));
    bool isSVG = costumeId.size() >= 4 &&
                 (costumeId.substr(costumeId.size() - 4) == ".svg" ||
                  costumeId.substr(costumeId.size() - 4) == ".SVG");

    if (!isBitmap && !isSVG) {
        Log::logWarning("File is not a supported image format: " + costumeId);
        return;
    }

    // Extract file data
    size_t file_size;
    void *file_data = mz_zip_reader_extract_to_heap(zip, file_index, &file_size, 0);
    if (!file_data) {
        Log::logWarning("Failed to extract: " + costumeId);
        return;
    }

    int width, height;
    unsigned char *rgba_data = nullptr;

    imageRGBA newRGBA;

    if (isSVG) {
        newRGBA.isSVG = true;
        rgba_data = SVGToRGBA(file_data, file_size, width, height);
        if (!rgba_data) {
            Log::logWarning("Failed to decode SVG: " + costumeId);
            mz_free(file_data);
            Image::cleanupImages();
            return;
        }
    } else {
        // Handle bitmap files (PNG, JPG)
        int channels;
        rgba_data = stbi_load_from_memory(
            (unsigned char *)file_data, file_size,
            &width, &height, &channels, 4);

        if (!rgba_data) {
            Log::logWarning("Failed to decode image: " + costumeId);
            mz_free(file_data);
            Image::cleanupImages();
            return;
        }
    }

    // Set up the image data structure
    newRGBA.name = imageId;
    newRGBA.fullName = costumeId;
    newRGBA.width = width;
    newRGBA.height = height;
    newRGBA.textureWidth = clamp(Math::next_pow2(newRGBA.width), 64, 1024);
    newRGBA.textureHeight = clamp(Math::next_pow2(newRGBA.height), 64, 1024);
    newRGBA.textureMemSize = newRGBA.textureWidth * newRGBA.textureHeight * 4;
    newRGBA.data = rgba_data;

    // Track memory usage
    size_t imageSize = width * height * 4;
    MemoryTracker::allocate(imageSize);

    // Log::log("Successfully loaded image!");
    imageRGBAS.push_back(newRGBA);

    // Clean up
    mz_free(file_data);
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

bool getImageFromT3x(const std::string &filePath) {
    std::string filename = filePath.substr(filePath.find_last_of('/') + 1);
    std::string path2 = filename.substr(0, filename.find_last_of('.'));

    C2D_SpriteSheet sheet = C2D_SpriteSheetLoad(filePath.c_str());
    if (!sheet) {
        // Log::logWarning("Could not load sprite from t3x!");
        return false;
    }

    // get first image from spritesheet since that's all we're using
    C2D_Image image = C2D_SpriteSheetGetImage(sheet, 0);

    imageRGBA newRGBA;
    newRGBA.width = image.subtex->width;
    newRGBA.height = image.subtex->height;
    newRGBA.fullName = filePath;
    newRGBA.name = path2;
    newRGBA.isSVG = false;
    newRGBA.textureWidth = clamp(Math::next_pow2(newRGBA.width), 64, 1024);
    newRGBA.textureHeight = clamp(Math::next_pow2(newRGBA.height), 64, 1024);
    newRGBA.textureMemSize = newRGBA.textureWidth * newRGBA.textureHeight * 4;
    newRGBA.data = nullptr;

    // Track memory usage
    size_t imageSize = newRGBA.width * newRGBA.height * 4;
    MemoryTracker::allocateVRAM(imageSize);

    // Log::log("Successfully loaded image from t3x!");
    imageRGBAS.push_back(newRGBA);

    imageC2Ds[newRGBA.name] = {image, 240, sheet};

    return true;
}

/**
 * Reads an `imageRGBA` image, and adds a `C2D_Image` object to `imageC2Ds`.
 * Assumes image data is stored left->right, top->bottom.
 * Dimensions must be within 64x64 and 1024x1024.
 * Code here originally from https://gbatemp.net/threads/citro2d-c2d_image-example.668574/
 * then edited to fit my code
 */
bool get_C2D_Image(imageRGBA rgba) {

    // u32 px_count = rgba.width * rgba.height;
    u32 *rgba_raw = reinterpret_cast<u32 *>(rgba.data);

    // Image data
    C2D_Image image;

    // Base texture
    C3D_Tex *tex = new C3D_Tex();
    // C3D_Tex *tex = MemoryTracker::allocate<C3D_Tex>();
    // new (tex) C3D_Tex();
    image.tex = tex;

    // Texture dimensions must be square powers of two between 64x64 and 1024x1024
    tex->width = rgba.textureWidth;
    tex->height = rgba.textureHeight;

    size_t textureSize = rgba.textureMemSize;

    // Subtexture
    Tex3DS_SubTexture *subtex = new Tex3DS_SubTexture();
    // Tex3DS_SubTexture *subtex = MemoryTracker::allocate<Tex3DS_SubTexture>();
    // new (subtex) Tex3DS_SubTexture();

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
        delete tex;
        delete subtex;
        // MemoryTracker::deallocate(tex);
        // MemoryTracker::deallocate(subtex);
        cleanupImagesLite();
        return false;
    }
    C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);

    if (!tex->data) {
        Log::logWarning("Texture data is null!");
        C3D_TexDelete(tex);
        delete tex;
        delete subtex;
        // MemoryTracker::deallocate(tex);
        // MemoryTracker::deallocate(subtex);
        cleanupImagesLite();
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

    // Log::log("C2D Image Successfully loaded!");

    MemoryTracker::allocateVRAM(rgba.textureMemSize);

    imageC2Ds[rgba.name] = {image};
    C3D_FrameSync(); // wait for Async functions to finish
    return true;
}

/**
 * Frees a `C2D_Image` from memory using `costumeId` string to find it.
 */
void Image::freeImage(const std::string &costumeId) {
    auto it = imageC2Ds.find(costumeId);
    if (it != imageC2Ds.end()) {
        // Log::log("freed image!");

        if (it->second.sheet) {
            if (it->second.image.tex) {
                size_t textureSize = it->second.image.subtex->width * it->second.image.subtex->height * 4;
                MemoryTracker::deallocateVRAM(textureSize);
            }

            C2D_SpriteSheetFree(it->second.sheet);

            // Log::log("Freed sprite sheet for: " + costumeId);
            goto afterFreeing;
        }

        if (it->second.image.tex) {
            C3D_TexDelete(it->second.image.tex);
            delete it->second.image.tex;
            it->second.image.tex = nullptr;
        }
        if (it->second.image.subtex) {
            delete it->second.image.subtex;
        }

    afterFreeing:

        imageC2Ds.erase(it);
    } else {
        Log::logWarning("cant find image to free: " + costumeId);
    }
    freeRGBA(costumeId);
}

void cleanupImagesLite() {
    std::vector<std::string> keysToDelete;
    keysToDelete.reserve(imageC2Ds.size());

    for (const auto &[id, data] : imageC2Ds) {
        if (data.freeTimer < data.maxFreeTimer * 0.8)
            keysToDelete.push_back(id);
    }

    for (const std::string &id : keysToDelete) {
        Image::freeImage(id);
    }
}

void Image::cleanupImages() {

    std::vector<std::string> keysToDelete;
    keysToDelete.reserve(imageC2Ds.size());

    for (const auto &[id, data] : imageC2Ds) {
        keysToDelete.push_back(id);
    }

    for (const std::string &id : keysToDelete) {
        freeImage(id);
    }

    // Clear maps & queues to prevent dangling references
    imageC2Ds.clear();
    imageLoadQueue.clear();
    toDelete.clear();

    // Log::log("Image cleanup completed.");
}

void freeRGBA(const std::string &imageName) {
    auto it = std::find_if(imageRGBAS.begin(), imageRGBAS.end(), [&](const imageRGBA &img) {
        return img.name == imageName;
    });

    if (it != imageRGBAS.end()) {
        if (it->data != nullptr && it->data) {
            size_t dataSize = it->width * it->height * 4;

            if (it->isSVG) {
                MemoryTracker::deallocate(it->data, dataSize);
            } else {
                MemoryTracker::deallocate(it->data, dataSize);
            }
            MemoryTracker::deallocateVRAM(it->textureMemSize);

            // Log::log("Freed RGBA data for " + imageName);
        }
        imageRGBAS.erase(it);
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
    std::vector<std::string> keysToDelete;

    // timer based freeing
    for (auto &[id, data] : imageC2Ds) {
        if (data.freeTimer <= 0) {
            keysToDelete.push_back(id);
        } else {
            data.freeTimer -= 1;
        }
    }

    for (const std::string &id : keysToDelete) {
        Image::freeImage(id);
    }
}
