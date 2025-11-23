#include "image.hpp"
#include "../scratch/image.hpp"
#include "miniz.h"
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

#ifdef __PC__
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

Image::Image(std::string filePath) {
    if (!loadImageFromFile(filePath, nullptr, false)) return;
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

    SDL_FPoint center = {image->renderRect.w / 2, image->renderRect.h / 2};

    image->freeTimer = image->maxFreeTime;
    SDL_RenderTextureRotated(renderer, image->spriteTexture, &image->textureRect, &image->renderRect, rotation, &center, SDL_FLIP_NONE);
}

// I doubt you want to mess with this...
void Image::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
    if (images.find(imageId) == images.end()) return;
    SDL_Image *image = images[imageId];

    image->setScale(1.0);
    image->setRotation(0.0);

    uint8_t alpha = static_cast<Uint8>(opacity * 255);
    SDL_SetTextureAlphaMod(image->spriteTexture, alpha);

    const float destX = xPos - (centered ? width / 2.0f : 0);
    const float destY = yPos - (centered ? height / 2.0f : 0);
    const float srcPadding = std::max(1.0f, std::min(std::min(static_cast<float>(padding), image->width / 2.0f), static_cast<float>(image->height)) / 2.0f);

    const float srcCenterWidth = std::max(0.0f, image->width - 2 * srcPadding);
    const float srcCenterHeight = std::max(0.0f, image->height - 2 * srcPadding);

    const SDL_FRect srcTopLeft = {0, 0, srcPadding, srcPadding};
    const SDL_FRect srcTop = {srcPadding, 0, srcCenterWidth, srcPadding};
    const SDL_FRect srcTopRight = {image->width - srcPadding, 0, srcPadding, srcPadding};
    const SDL_FRect srcLeft = {0, srcPadding, srcPadding, srcCenterHeight};
    const SDL_FRect srcCenter = {srcPadding, srcPadding, srcCenterWidth, srcCenterHeight};
    const SDL_FRect srcRight = {image->width - srcPadding, srcPadding, srcPadding, srcCenterHeight};
    const SDL_FRect srcBottomLeft = {0, image->height - srcPadding, srcPadding, srcPadding};
    const SDL_FRect srcBottom = {srcPadding, image->height - srcPadding, srcCenterWidth, srcPadding};
    const SDL_FRect srcBottomRight = {image->width - srcPadding, image->height - srcPadding, srcPadding, srcPadding};

    const float dstCenterWidth = std::max(0.0f, static_cast<float>(width) - 2 * srcPadding);
    const float dstCenterHeight = std::max(0.0f, static_cast<float>(height) - 2 * srcPadding);

    const SDL_FRect dstTopLeft = {destX, destY, srcPadding, srcPadding};
    const SDL_FRect dstTop = {destX + srcPadding, destY, dstCenterWidth, srcPadding};
    const SDL_FRect dstTopRight = {destX + srcPadding + dstCenterWidth, destY, srcPadding, srcPadding};

    const SDL_FRect dstLeft = {destX, destY + srcPadding, srcPadding, dstCenterHeight};
    const SDL_FRect dstCenter = {destX + srcPadding, destY + srcPadding, dstCenterWidth, dstCenterHeight};
    const SDL_FRect dstRight = {destX + srcPadding + dstCenterWidth, destY + srcPadding, srcPadding, dstCenterHeight};
    const SDL_FRect dstBottomLeft = {destX, destY + srcPadding + dstCenterHeight, srcPadding, srcPadding};
    const SDL_FRect dstBottom = {destX + srcPadding, destY + srcPadding + dstCenterHeight, dstCenterWidth, srcPadding};
    const SDL_FRect dstBottomRight = {destX + srcPadding + dstCenterWidth, destY + srcPadding + dstCenterHeight, srcPadding, srcPadding};

    image->freeTimer = image->maxFreeTime;

    SDL_Texture *originalTexture = image->spriteTexture;
    SDL_ScaleMode originalScaleMode;
    SDL_GetTextureScaleMode(originalTexture, &originalScaleMode);
    SDL_SetTextureScaleMode(originalTexture, SDL_SCALEMODE_NEAREST);

    SDL_RenderTexture(renderer, originalTexture, &srcTopLeft, &dstTopLeft);
    SDL_RenderTexture(renderer, originalTexture, &srcTop, &dstTop);
    SDL_RenderTexture(renderer, originalTexture, &srcTopRight, &dstTopRight);
    SDL_RenderTexture(renderer, originalTexture, &srcLeft, &dstLeft);
    SDL_RenderTexture(renderer, originalTexture, &srcCenter, &dstCenter);
    SDL_RenderTexture(renderer, originalTexture, &srcRight, &dstRight);
    SDL_RenderTexture(renderer, originalTexture, &srcBottomLeft, &dstBottomLeft);
    SDL_RenderTexture(renderer, originalTexture, &srcBottom, &dstBottom);
    SDL_RenderTexture(renderer, originalTexture, &srcBottomRight, &dstBottomRight);

    SDL_SetTextureScaleMode(originalTexture, originalScaleMode);
}

/**
 * Loads a single `SDL_Image` from an unzipped filepath .
 * @param filePath
 */
bool Image::loadImageFromFile(std::string filePath, Sprite *sprite, bool fromScratchProject) {
    std::string imgId = filePath.substr(0, filePath.find_last_of('.'));
    if (images.find(imgId) != images.end()) return true;

    std::string finalPath;

    finalPath = OS::getRomFSLocation();
    if (fromScratchProject)
        finalPath = finalPath + "project/";

    finalPath = finalPath + filePath;
    if (Unzip::UnpackedInSD) finalPath = Unzip::filePath + filePath;
    // SDL_Image *image = new SDL_Image(finalPath);
    SDL_Image *image = new SDL_Image();
    new (image) SDL_Image(finalPath);

    // Track texture memory
    if (image->spriteTexture) {
        size_t textureMemory = image->width * image->height * 4;
        image->memorySize = textureMemory;
    }

    if (sprite != nullptr) {
        sprite->spriteWidth = image->textureRect.w / 2;
        sprite->spriteHeight = image->textureRect.h / 2;
    }

    images[imgId] = image;
    return true;
}

/**
 * Loads a single image from a Scratch sb3 zip file by filename.
 * @param zip Pointer to the zip archive
 * @param costumeId The filename of the image to load (e.g., "sprite1.png")
 */
void Image::loadImageFromSB3(mz_zip_archive *zip, const std::string &costumeId, Sprite *sprite) {
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

    // Use SDL_IOStream to load image from memory
    SDL_IOStream *io = SDL_IOFromMem(file_data, file_size);
    if (!io) {
        Log::logWarning("Failed to create RWops for: " + costumeId);
        mz_free(file_data);
        return;
    }

    SDL_Surface *surface = IMG_Load_IO(io, 0);
    SDL_CloseIO(io);
    mz_free(file_data);

    if (!surface) {
        Log::logWarning("Failed to load image from memory: " + costumeId);
        Log::logWarning("IMG Error: " + std::string(SDL_GetError()));
        return;
    }

// PS4 piglet expects RGBA instead of ABGR.
#if defined(__PS4__)
    SDL_Surface *convert = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888, 0);
    if (convert == NULL) {
        Log::logWarning(std::string("Error converting image surface: ") + SDL_GetError());
        SDL_FreeSurface(convert);
        return;
    }

    SDL_FreeSurface(surface);
    surface = convert;
#endif

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        Log::logWarning("Failed to create texture: " + costumeId);
        SDL_DestroySurface(surface);
        return;
    }
    SDL_DestroySurface(surface);

    // Build SDL_Image object
    SDL_Image *image = new SDL_Image();
    new (image) SDL_Image();
    image->spriteTexture = texture;
    SDL_GetTextureSize(texture, &image->width, &image->height);
    image->renderRect = {0, 0, image->width, image->height};
    image->textureRect = {0, 0, image->width, image->height};

    // calculate VRAM usage
    const SDL_PropertiesID props = SDL_GetTextureProperties(texture);
    const SDL_PixelFormat format = static_cast<SDL_PixelFormat>(SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_FORMAT_NUMBER, 0));
    const int w = SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_WIDTH_NUMBER, 0);
    const int h = SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_HEIGHT_NUMBER, 0);
    int bpp;
    Uint32 Rmask, Gmask, Bmask, Amask;
    if (SDL_GetMasksForPixelFormat(format, &bpp, &Rmask, &Gmask, &Bmask, &Amask) == 0) {
        image->memorySize = w * h * bpp;
    }

    if (sprite != nullptr) {
        sprite->spriteWidth = image->textureRect.w / 2;
        sprite->spriteHeight = image->textureRect.h / 2;
    }

    // Log::log("Successfully loaded image: " + costumeId);
    images[imgId] = image;
}

void Image::cleanupImages() {
    for (auto &[id, image] : images)
        delete image;
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
        delete image;

        images.erase(imageIt);
    }
}

/**
 * Checks every `SDL_Image` in memory to see if they can be freed.
 * An `SDL_Image` will get freed if it goes unused for 120 frames.
 */
void Image::FlushImages() {

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

SDL_Image::SDL_Image() {}

SDL_Image::SDL_Image(std::string filePath) {
#ifdef __PC__
    const auto &file = cmrc::romfs::get_filesystem().open(filePath);
    spriteSurface = IMG_Load_IO(SDL_IOFromConstMem(file.begin(), file.size()), 1);
#else
    spriteSurface = IMG_Load(filePath.c_str());
#endif
    if (spriteSurface == NULL) {
        Log::logWarning(std::string("Error loading image: ") + SDL_GetError());
        return;
    }

    spriteTexture = SDL_CreateTextureFromSurface(renderer, spriteSurface);
    if (spriteTexture == NULL) {
        Log::logWarning(std::string("Error creating texture: ") + SDL_GetError());
        return;
    }
    SDL_DestroySurface(spriteSurface);

    // get width and height of image
    SDL_GetTextureSize(spriteTexture, &width, &height);
    renderRect.w = width;
    renderRect.h = height;
    textureRect.w = width;
    textureRect.h = height;
    textureRect.x = 0;
    textureRect.y = 0;

    // calculate VRAM usage
    int bpp;
    Uint32 Rmask, Gmask, Bmask, Amask;
    if (SDL_GetMasksForPixelFormat(static_cast<SDL_PixelFormat>(SDL_GetNumberProperty(SDL_GetTextureProperties(spriteTexture), SDL_PROP_TEXTURE_FORMAT_NUMBER, 0)), &bpp, &Rmask, &Gmask, &Bmask, &Amask) == 0) {
        memorySize = (width * height * bpp) / 8;
    }

    // Log::log("Image loaded!");
}

/**
 * currently does nothing in the SDL version 😁😁
 */
void Image::queueFreeImage(const std::string &costumeId) {
    toDelete.push_back(costumeId);
}

SDL_Image::~SDL_Image() {
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
