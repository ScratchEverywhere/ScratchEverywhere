#include "image.hpp"
#include "../scratch/image.hpp"
#include "miniz/miniz.h"
#include "os.hpp"
#include "render.hpp"
#include "unzip.hpp"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

std::unordered_map<std::string, SDL_Image *> images;
static std::vector<std::string> toDelete;

Image::Image(std::string filePath) {
    if (!loadImageFromFile(filePath, false)) return;
    std::string imgId = filePath.substr(0, filePath.find_last_of('.'));
    imageId = imgId;
    width = images[imgId]->width;
    height = images[imgId]->height;
    scale = 1.0;
    rotation = 0.0;
    opacity = 1.0;
    images[imgId]->imageUsageCount++;
}

Image::~Image() {
    auto it = images.find(imageId);
    if (it != images.end()) {
        images[imageId]->imageUsageCount--;
        if (images[imageId]->imageUsageCount <= 0)
            freeImage(imageId);
    }
}

void Image::render(double xPos, double yPos, bool centered) {
    if (images.find(imageId) == images.end()) return;
    SDL_Image *image = images[imageId];

    image->setScale(scale);
    image->setRotation(rotation);

    if (centered) {
        image->renderRect.x = xPos - (image->renderRect.w / 2);
        image->renderRect.y = yPos - (image->renderRect.h / 2);
    } else {
        image->renderRect.x = xPos;
        image->renderRect.y = yPos;
    }

    Uint8 alpha = static_cast<Uint8>(opacity * 255);
    SDL_SetTextureAlphaMod(image->spriteTexture, alpha);

    SDL_Point center = {image->renderRect.w / 2, image->renderRect.h / 2};

    image->freeTimer = image->maxFreeTime;
    SDL_RenderCopyEx(renderer, image->spriteTexture, &image->textureRect, &image->renderRect, rotation, &center, SDL_FLIP_NONE);
}

// I doubt you want to mess with this...
void Image::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
    if (images.find(imageId) == images.end()) return;
    SDL_Image *image = images[imageId];

    image->setScale(1.0);
    image->setRotation(0.0);

    uint8_t alpha = static_cast<Uint8>(opacity * 255);
    SDL_SetTextureAlphaMod(image->spriteTexture, alpha);

    const int iDestX = static_cast<int>(xPos - (centered ? width / 2 : 0));
    const int iDestY = static_cast<int>(yPos - (centered ? height / 2 : 0));
    const int iWidth = static_cast<int>(width);
    const int iHeight = static_cast<int>(height);
    const int iSrcPadding = std::max(1, static_cast<int>(std::min(std::min(padding, static_cast<double>(image->width) / 2), static_cast<double>(image->height) / 2)));

    const int srcCenterWidth = std::max(0, image->width - 2 * iSrcPadding);
    const int srcCenterHeight = std::max(0, image->height - 2 * iSrcPadding);

    const SDL_Rect srcTopLeft = {0, 0, iSrcPadding, iSrcPadding};
    const SDL_Rect srcTop = {iSrcPadding, 0, srcCenterWidth, iSrcPadding};
    const SDL_Rect srcTopRight = {image->width - iSrcPadding, 0, iSrcPadding, iSrcPadding};
    const SDL_Rect srcLeft = {0, iSrcPadding, iSrcPadding, srcCenterHeight};
    const SDL_Rect srcCenter = {iSrcPadding, iSrcPadding, srcCenterWidth, srcCenterHeight};
    const SDL_Rect srcRight = {image->width - iSrcPadding, iSrcPadding, iSrcPadding, srcCenterHeight};
    const SDL_Rect srcBottomLeft = {0, image->height - iSrcPadding, iSrcPadding, iSrcPadding};
    const SDL_Rect srcBottom = {iSrcPadding, image->height - iSrcPadding, srcCenterWidth, iSrcPadding};
    const SDL_Rect srcBottomRight = {image->width - iSrcPadding, image->height - iSrcPadding, iSrcPadding, iSrcPadding};

    const int dstCenterWidth = std::max(0, iWidth - 2 * iSrcPadding);
    const int dstCenterHeight = std::max(0, iHeight - 2 * iSrcPadding);

    const SDL_Rect dstTopLeft = {iDestX, iDestY, iSrcPadding, iSrcPadding};
    const SDL_Rect dstTop = {iDestX + iSrcPadding, iDestY, dstCenterWidth, iSrcPadding};
    const SDL_Rect dstTopRight = {iDestX + iSrcPadding + dstCenterWidth, iDestY, iSrcPadding, iSrcPadding};

    const SDL_Rect dstLeft = {iDestX, iDestY + iSrcPadding, iSrcPadding, dstCenterHeight};
    const SDL_Rect dstCenter = {iDestX + iSrcPadding, iDestY + iSrcPadding, dstCenterWidth, dstCenterHeight};
    const SDL_Rect dstRight = {iDestX + iSrcPadding + dstCenterWidth, iDestY + iSrcPadding, iSrcPadding, dstCenterHeight};
    const SDL_Rect dstBottomLeft = {iDestX, iDestY + iSrcPadding + dstCenterHeight, iSrcPadding, iSrcPadding};
    const SDL_Rect dstBottom = {iDestX + iSrcPadding, iDestY + iSrcPadding + dstCenterHeight, dstCenterWidth, iSrcPadding};
    const SDL_Rect dstBottomRight = {iDestX + iSrcPadding + dstCenterWidth, iDestY + iSrcPadding + dstCenterHeight, iSrcPadding, iSrcPadding};

    image->freeTimer = image->maxFreeTime;

    SDL_Texture *originalTexture = image->spriteTexture;
    SDL_ScaleMode originalScaleMode;
    SDL_GetTextureScaleMode(originalTexture, &originalScaleMode);
    SDL_SetTextureScaleMode(originalTexture, SDL_ScaleModeNearest);

    SDL_RenderCopy(renderer, originalTexture, &srcTopLeft, &dstTopLeft);
    SDL_RenderCopy(renderer, originalTexture, &srcTop, &dstTop);
    SDL_RenderCopy(renderer, originalTexture, &srcTopRight, &dstTopRight);
    SDL_RenderCopy(renderer, originalTexture, &srcLeft, &dstLeft);
    SDL_RenderCopy(renderer, originalTexture, &srcCenter, &dstCenter);
    SDL_RenderCopy(renderer, originalTexture, &srcRight, &dstRight);
    SDL_RenderCopy(renderer, originalTexture, &srcBottomLeft, &dstBottomLeft);
    SDL_RenderCopy(renderer, originalTexture, &srcBottom, &dstBottom);
    SDL_RenderCopy(renderer, originalTexture, &srcBottomRight, &dstBottomRight);

    SDL_SetTextureScaleMode(originalTexture, originalScaleMode);
}

/**
 * Loads a single `SDL_Image` from an unzipped filepath .
 * @param filePath
 */
bool Image::loadImageFromFile(std::string filePath, bool fromScratchProject) {
    std::string imgId = filePath.substr(0, filePath.find_last_of('.'));
    if (images.find(imgId) != images.end()) return true;

    std::string finalPath;

    finalPath = OS::getRomFSLocation();
    if (fromScratchProject)
        finalPath = finalPath + "project/";

    finalPath = finalPath + filePath;
    if (Unzip::UnpackedInSD) finalPath = Unzip::filePath + filePath;
    // SDL_Image *image = new SDL_Image(finalPath);
    SDL_Image *image = MemoryTracker::allocate<SDL_Image>();
    new (image) SDL_Image(finalPath);

    // Track texture memory
    if (image->spriteTexture) {
        size_t textureMemory = image->width * image->height * 4;
        MemoryTracker::allocateVRAM(textureMemory);
        image->memorySize = textureMemory;
    }

    images[imgId] = image;
    return true;
}

/**
 * Loads a single image from a Scratch sb3 zip file by filename.
 * @param zip Pointer to the zip archive
 * @param costumeId The filename of the image to load (e.g., "sprite1.png")
 */
void Image::loadImageFromSB3(mz_zip_archive *zip, const std::string &costumeId) {
    std::string imgId = costumeId.substr(0, costumeId.find_last_of('.'));
    if (images.find(imgId) != images.end()) return;

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
    bool isSupported = costumeId.size() > 4 && ([](std::string ext) {
                           std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                           return ext == ".bmp" || ext == ".gif" || ext == ".jpg" || ext == ".jpeg" ||
                                  ext == ".lbm" || ext == ".iff" || ext == ".pcx" || ext == ".png" ||
                                  ext == ".pnm" || ext == ".ppm" || ext == ".pgm" || ext == ".pbm" ||
                                  ext == ".qoi" || ext == ".tga" || ext == ".tiff" || ext == ".xcf" ||
                                  ext == ".xpm" || ext == ".xv" || ext == ".ico" || ext == ".cur" ||
                                  ext == ".ani" || ext == ".webp" || ext == ".avif" || ext == ".jxl" ||
                                  ext == ".svg";
                       }(costumeId.substr(costumeId.find_last_of('.'))));

    if (!isSupported) {
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

    // Use SDL_RWops to load image from memory
    SDL_RWops *rw = SDL_RWFromMem(file_data, file_size);
    if (!rw) {
        Log::logWarning("Failed to create RWops for: " + costumeId);
        mz_free(file_data);
        return;
    }

    SDL_Surface *surface = IMG_Load_RW(rw, 0);
    SDL_RWclose(rw);
    mz_free(file_data);

    if (!surface) {
        Log::logWarning("Failed to load image from memory: " + costumeId);
        Log::logWarning("IMG Error: " + std::string(IMG_GetError()));
        return;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        Log::logWarning("Failed to create texture: " + costumeId);
        SDL_FreeSurface(surface);
        return;
    }

    SDL_FreeSurface(surface);

    // Build SDL_Image object
    SDL_Image *image = MemoryTracker::allocate<SDL_Image>();
    new (image) SDL_Image();
    image->spriteTexture = texture;
    SDL_QueryTexture(texture, nullptr, nullptr, &image->width, &image->height);
    image->renderRect = {0, 0, image->width, image->height};
    image->textureRect = {0, 0, image->width, image->height};

    // calculate VRAM usage
    Uint32 format;
    int w, h;
    SDL_QueryTexture(texture, &format, NULL, &w, &h);
    int bpp;
    Uint32 Rmask, Gmask, Bmask, Amask;
    SDL_PixelFormatEnumToMasks(format, &bpp, &Rmask, &Gmask, &Bmask, &Amask);
    image->memorySize = (w * h * bpp) / 8;
    MemoryTracker::allocateVRAM(image->memorySize);

    // Log::log("Successfully loaded image: " + costumeId);
    images[imgId] = image;
}

void Image::cleanupImages() {
    for (auto &[id, image] : images) {
        if (image->memorySize > 0) {
            MemoryTracker::deallocateVRAM(image->memorySize);
        }
        // delete image;
        image->~SDL_Image();
        MemoryTracker::deallocate<SDL_Image>(image);
    }
    images.clear();
    toDelete.clear();
}

/**
 * Frees an `SDL_Image` from memory using a `costumeId` to find it.
 * @param costumeId
 */
void Image::freeImage(const std::string &costumeId) {
    auto imageIt = images.find(costumeId);
    if (imageIt != images.end()) {
        SDL_Image *image = imageIt->second;

        // Log::log("Freed image " + costumeId);
        //  Call destructor and deallocate SDL_Image
        image->~SDL_Image();
        MemoryTracker::deallocate<SDL_Image>(image);

        images.erase(imageIt);
    }
}

/**
 * Checks every `SDL_Image` in memory to see if they can be freed.
 * An `SDL_Image` will get freed if it goes unused for 120 frames.
 */
void Image::FlushImages() {

    // Free images if ram usage is too high
    if (MemoryTracker::getVRAMUsage() + MemoryTracker::getCurrentUsage() > MemoryTracker::getMaxVRAMUsage() * 0.8) {

        size_t times = 0;
        while (MemoryTracker::getVRAMUsage() + MemoryTracker::getCurrentUsage() > MemoryTracker::getMaxVRAMUsage() * 0.5 && !images.empty()) {
            SDL_Image *imgToDelete = nullptr;
            std::string toDeleteStr = "";

            for (auto &[id, img] : images) {
                if (imgToDelete == nullptr && img->freeTimer != img->maxFreeTime) {
                    imgToDelete = img;
                    toDeleteStr = id;
                    continue;
                }
                if (imgToDelete != nullptr && img->freeTimer < imgToDelete->freeTimer && img->freeTimer != img->maxFreeTime) {
                    imgToDelete = img;
                    toDeleteStr = id;
                }
            }

            if (toDeleteStr != "") {
                Image::freeImage(toDeleteStr);
            } else {
                break;
            }
            times++;
            if (times > 15) break;
        }
    } else {
        // Free images based on a timer
        for (auto &[id, img] : images) {
            if (img->freeTimer <= 0) {
                toDelete.push_back(id);
            } else {
                img->freeTimer -= 1;
            }
        }

        for (const std::string &id : toDelete) {
            Image::freeImage(id);
        }
        toDelete.clear();
    }
}

SDL_Image::SDL_Image() {}

SDL_Image::SDL_Image(std::string filePath) {
    spriteSurface = IMG_Load(filePath.c_str());
    if (spriteSurface == NULL) {
        Log::logWarning(std::string("Error loading image: ") + IMG_GetError());
        return;
    }
    spriteTexture = SDL_CreateTextureFromSurface(renderer, spriteSurface);
    if (spriteTexture == NULL) {
        Log::logWarning("Error creating texture");
        return;
    }
    SDL_FreeSurface(spriteSurface);

    // get width and height of image
    int texW = 0;
    int texH = 0;
    SDL_QueryTexture(spriteTexture, NULL, NULL, &texW, &texH);
    width = texW;
    height = texH;
    renderRect.w = width;
    renderRect.h = height;
    textureRect.w = width;
    textureRect.h = height;
    textureRect.x = 0;
    textureRect.y = 0;

    // calculate VRAM usage
    Uint32 format;
    int w, h;
    SDL_QueryTexture(spriteTexture, &format, NULL, &w, &h);
    int bpp;
    Uint32 Rmask, Gmask, Bmask, Amask;
    SDL_PixelFormatEnumToMasks(format, &bpp, &Rmask, &Gmask, &Bmask, &Amask);
    memorySize = (w * h * bpp) / 8;
    MemoryTracker::allocateVRAM(memorySize);

    // Log::log("Image loaded!");
}

/**
 * currently does nothing in the SDL version 😁😁
 */
void Image::queueFreeImage(const std::string &costumeId) {
    toDelete.push_back(costumeId);
}

SDL_Image::~SDL_Image() {
    MemoryTracker::deallocateVRAM(memorySize);
    SDL_DestroyTexture(spriteTexture);
}

void SDL_Image::setScale(float amount) {
    scale = amount;
    renderRect.w = width * amount;
    renderRect.h = height * amount;
}

void SDL_Image::setRotation(float rotate) {
    rotation = rotate;
}
