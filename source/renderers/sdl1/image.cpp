#include "image.hpp"
#include <SDL/SDL_rotozoom.h>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <image.hpp>
#include <miniz.h>
#include <os.hpp>
#include <render.hpp>
#include <string>
#include <unordered_map>
#include <unzip.hpp>
#include <vector>
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

std::unordered_map<std::string, SDL_Image *> images;
static std::vector<std::string> toDelete;

#ifdef USE_CMAKERC
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
    SDL_SetAlpha(image->spriteTexture, SDL_SRCALPHA, alpha);

    image->freeTimer = image->maxFreeTime;

    SDL_Surface *rotatedSurface = rotozoomSurface(image->spriteTexture, rotation, scale, SMOOTHING_OFF);
    if (rotatedSurface) {
        SDL_Rect destRect = {
            static_cast<Sint16>(xPos - (centered ? rotatedSurface->w / 2 : 0)),
            static_cast<Sint16>(yPos - (centered ? rotatedSurface->h / 2 : 0)),
            0, 0};
        SDL_BlitSurface(rotatedSurface, NULL, static_cast<SDL_Surface *>(Render::getRenderer()), &destRect);
        SDL_FreeSurface(rotatedSurface);
    } else {
        SDL_BlitSurface(image->spriteTexture, NULL, static_cast<SDL_Surface *>(Render::getRenderer()), &image->renderRect);
    }
}

// FIXME: SDL_BlitSurface doesn't have support for scaling. Omit 9-slice rendering for now.
void Image::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
#if 1
    if (images.find(imageId) == images.end()) return;
    SDL_Image *image = images[imageId];

    scale = std::min(width / image->width, height / image->height);
    image->setScale(scale);
    render(xPos, yPos, centered);
#else
    if (images.find(imageId) == images.end()) return;
    SDL_Image *image = images[imageId];

    image->setScale(1.0);
    image->setRotation(0.0);

    uint8_t alpha = static_cast<Uint8>(opacity * 255);
    SDL_SetAlpha(image->spriteTexture, SDL_SRCALPHA, alpha);

    const Uint16 iDestX = static_cast<Uint16>(xPos - (centered ? width / 2 : 0));
    const Uint16 iDestY = static_cast<Uint16>(yPos - (centered ? height / 2 : 0));
    const Uint16 iWidth = static_cast<Uint16>(width);
    const Uint16 iHeight = static_cast<Uint16>(height);
    const Uint16 iSrcPadding = std::max(1, static_cast<int>(std::min(std::min(padding, static_cast<double>(image->width) / 2), static_cast<double>(image->height) / 2)));

    const Uint16 srcCenterWidth = std::max(0, image->width - 2 * iSrcPadding);
    const Uint16 srcCenterHeight = std::max(0, image->height - 2 * iSrcPadding);

    SDL_Rect srcTopLeft = {0, 0, iSrcPadding, iSrcPadding};
    SDL_Rect srcTop = {static_cast<Sint16>(iSrcPadding), 0, srcCenterWidth, iSrcPadding};
    SDL_Rect srcTopRight = {static_cast<Sint16>(image->width - iSrcPadding), 0, iSrcPadding, iSrcPadding};
    SDL_Rect srcLeft = {0, static_cast<Sint16>(iSrcPadding), iSrcPadding, srcCenterHeight};
    SDL_Rect srcCenter = {static_cast<Sint16>(iSrcPadding), static_cast<Sint16>(iSrcPadding), srcCenterWidth, srcCenterHeight};
    SDL_Rect srcRight = {static_cast<Sint16>(image->width - iSrcPadding), static_cast<Sint16>(iSrcPadding), iSrcPadding, srcCenterHeight};
    SDL_Rect srcBottomLeft = {0, static_cast<Sint16>(image->height - iSrcPadding), iSrcPadding, iSrcPadding};
    SDL_Rect srcBottom = {static_cast<Sint16>(iSrcPadding), static_cast<Sint16>(image->height - iSrcPadding), srcCenterWidth, iSrcPadding};
    SDL_Rect srcBottomRight = {static_cast<Sint16>(image->width - iSrcPadding), static_cast<Sint16>(image->height - iSrcPadding), iSrcPadding, iSrcPadding};

    const Uint16 dstCenterWidth = std::max(0, iWidth - 2 * iSrcPadding);
    const Uint16 dstCenterHeight = std::max(0, iHeight - 2 * iSrcPadding);

    SDL_Rect dstTopLeft = {static_cast<Sint16>(iDestX), static_cast<Sint16>(iDestY), iSrcPadding, iSrcPadding};
    SDL_Rect dstTop = {static_cast<Sint16>(iDestX + iSrcPadding), static_cast<Sint16>(iDestY), dstCenterWidth, iSrcPadding};
    SDL_Rect dstTopRight = {static_cast<Sint16>(iDestX + iSrcPadding + dstCenterWidth), static_cast<Sint16>(iDestY), iSrcPadding, iSrcPadding};

    SDL_Rect dstLeft = {static_cast<Sint16>(iDestX), static_cast<Sint16>(iDestY + iSrcPadding), iSrcPadding, dstCenterHeight};
    SDL_Rect dstCenter = {static_cast<Sint16>(iDestX + iSrcPadding), static_cast<Sint16>(iDestY + iSrcPadding), dstCenterWidth, dstCenterHeight};
    SDL_Rect dstRight = {static_cast<Sint16>(iDestX + iSrcPadding + dstCenterWidth), static_cast<Sint16>(iDestY + iSrcPadding), iSrcPadding, dstCenterHeight};
    SDL_Rect dstBottomLeft = {static_cast<Sint16>(iDestX), static_cast<Sint16>(iDestY + iSrcPadding + dstCenterHeight), iSrcPadding, iSrcPadding};
    SDL_Rect dstBottom = {static_cast<Sint16>(iDestX + iSrcPadding), static_cast<Sint16>(iDestY + iSrcPadding + dstCenterHeight), dstCenterWidth, iSrcPadding};
    SDL_Rect dstBottomRight = {static_cast<Sint16>(iDestX + iSrcPadding + dstCenterWidth), static_cast<Sint16>(iDestY + iSrcPadding + dstCenterHeight), iSrcPadding, iSrcPadding};

    image->freeTimer = image->maxFreeTime;

    SDL_Surface *renderer = static_cast<SDL_Surface *>(Render::getRenderer());

    SDL_BlitSurface(image->spriteTexture, &srcTopLeft, renderer, &dstTopLeft);
    SDL_BlitSurface(image->spriteTexture, &srcTop, renderer, &dstTop);
    SDL_BlitSurface(image->spriteTexture, &srcTopRight, renderer, &dstTopRight);
    SDL_BlitSurface(image->spriteTexture, &srcLeft, renderer, &dstLeft);
    SDL_BlitSurface(image->spriteTexture, &srcCenter, renderer, &dstCenter);
    SDL_BlitSurface(image->spriteTexture, &srcRight, renderer, &dstRight);
    SDL_BlitSurface(image->spriteTexture, &srcBottomLeft, renderer, &dstBottomLeft);
    SDL_BlitSurface(image->spriteTexture, &srcBottom, renderer, &dstBottom);
    SDL_BlitSurface(image->spriteTexture, &srcBottomRight, renderer, &dstBottomRight);
#endif
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
    if (Unzip::UnpackedInSD && fromScratchProject) finalPath = Unzip::filePath + filePath;
    SDL_Image *image = new SDL_Image(finalPath, fromScratchProject);

    if (!image->spriteTexture || image->spriteTexture == NULL) {
        return false;
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
                                  ext == ".ani" || ext == ".webp" || ext == ".svg";
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

    SDL_Surface *formattedSurface = nullptr;
    bool isSVG = costumeId.size() >= 4 && (costumeId.substr(costumeId.size() - 4) == ".svg");
    if (isSVG) {
        formattedSurface = SVGToSurface((char *)file_data, file_size);
        mz_free(file_data);
    } else {
        SDL_RWops *rw = SDL_RWFromMem(file_data, file_size);
        if (!rw) {
            Log::logWarning("Failed to create RWops for: " + costumeId);
            mz_free(file_data);
            return;
        }

        SDL_Surface *surface = IMG_Load_RW(rw, 1);
        mz_free(file_data);
        if (!surface) {
            Log::logWarning("Failed to load image from memory: " + costumeId);
            Log::logWarning("IMG Error: " + std::string(IMG_GetError()));
            return;
        }
        formattedSurface = SDL_DisplayFormatAlpha(surface);
        SDL_FreeSurface(surface);
    }

    if (!formattedSurface) {
        Log::logWarning("Failed to convert surface: " + costumeId);
        return;
    }

    // Build SDL_Image object
    SDL_Image *image = new SDL_Image();
    image->spriteTexture = formattedSurface;
    image->width = formattedSurface->w;
    image->height = formattedSurface->h;
    image->renderRect = {0, 0, (Uint16)image->width, (Uint16)image->height};
    image->textureRect = {0, 0, (Uint16)image->width, (Uint16)image->height};

    if (sprite != nullptr) {
        sprite->spriteWidth = image->textureRect.w / 2;
        sprite->spriteHeight = image->textureRect.h / 2;
    }

    // Log::log("Successfully loaded image: " + costumeId);
    images[imgId] = image;
}

SDL_Surface *SVGToSurface(const char *svg_data, size_t svg_size) {
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
    int width, height;
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

    // Calculate scale
    float scale = 1.0f;
    if (image->width > 0 && image->height > 0) {
        float scaleX = (float)width / image->width;
        float scaleY = (float)height / image->height;
        scale = std::min(scaleX, scaleY);
    }

    // Rasterize SVG
    nsvgRasterize(rast, image, 0, 0, scale, rgba_data, width, height, width * 4);

    // Create surface
    SDL_Surface *temp_surface = SDL_CreateRGBSurfaceFrom(rgba_data, width, height, 32, width * 4, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
    if (!temp_surface) {
        free(rgba_data);
        return nullptr;
    }

    // Format surface
    SDL_Surface *formattedSurface = SDL_DisplayFormatAlpha(temp_surface);

    // Clean up
    nsvgDeleteRasterizer(rast);
    nsvgDelete(image);
    SDL_FreeSurface(temp_surface);
    free(rgba_data);

    return formattedSurface;
}

void Image::cleanupImages() {
    for (auto &[id, image] : images) {
        delete image;
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

SDL_Image::SDL_Image(std::string filePath, bool fromScratchProject) {
    bool isSVG = filePath.size() >= 4 && (filePath.substr(filePath.size() - 4) == ".svg");

    if (isSVG) {
        char *svg_data;
        size_t size;
#ifdef USE_CMAKERC
        if (!Unzip::UnpackedInSD || !fromScratchProject) {
            const auto &file = cmrc::romfs::get_filesystem().open(filePath);
            svg_data = (char *)(malloc(file.size() + 1));
            std::copy(file.begin(), file.end(), svg_data);
            size = file.size();
        } else {
            FILE *fp = fopen(filePath.c_str(), "rb");
            if (!fp) {
                Log::logWarning("Failed to open SVG file: " + filePath);
                return;
            }
            fseek(fp, 0, SEEK_END);
            size = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            svg_data = (char *)malloc(size + 1);
            fread(svg_data, 1, size, fp);
            fclose(fp);
            svg_data[size] = 0;
        }
#else
        FILE *fp = fopen(filePath.c_str(), "rb");
        if (!fp) {
            Log::logWarning("Failed to open SVG file: " + filePath);
            return;
        }
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        svg_data = (char *)malloc(size + 1);
        fread(svg_data, 1, size, fp);
        fclose(fp);
        svg_data[size] = 0;
#endif
        spriteTexture = SVGToSurface(svg_data, size);
        free(svg_data);
    } else {
#ifdef USE_CMAKERC
        if (!Unzip::UnpackedInSD || !fromScratchProject) {
            const auto &file = cmrc::romfs::get_filesystem().open(filePath);
            spriteSurface = IMG_Load_RW(SDL_RWFromConstMem(file.begin(), file.size()), 1);
        } else {
            spriteSurface = IMG_Load(filePath.c_str());
        }
#else
        spriteSurface = IMG_Load(filePath.c_str());
#endif
        if (spriteSurface == NULL) {
            Log::logWarning(std::string("Error loading image: ") + IMG_GetError());
            return;
        }

        spriteTexture = SDL_DisplayFormatAlpha(spriteSurface);
        SDL_FreeSurface(spriteSurface);
        spriteSurface = nullptr;
    }

    if (spriteTexture == NULL) {
        Log::logWarning(std::string("Error creating texture for ") + filePath);
        return;
    }

    // get width and height of image
    width = spriteTexture->w;
    height = spriteTexture->h;
    renderRect.w = width;
    renderRect.h = height;
    textureRect.w = width;
    textureRect.h = height;
    textureRect.x = 0;
    textureRect.y = 0;

    // Log::log("Image loaded!");
}

/**
 * currently does nothing in the SDL version 😁😁
 */
void Image::queueFreeImage(const std::string &costumeId) {
    toDelete.push_back(costumeId);
}

SDL_Image::~SDL_Image() {
    SDL_FreeSurface(spriteTexture);
}

void SDL_Image::setScale(float amount) {
    scale = amount;
    renderRect.w = width * amount;
    renderRect.h = height * amount;
}

void SDL_Image::setRotation(float rotate) {
    rotation = rotate;
}
