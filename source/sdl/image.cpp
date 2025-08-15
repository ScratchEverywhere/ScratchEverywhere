#include "../scratch/image.hpp"
#include "../scratch/os.hpp"
#include "image.hpp"
#include "render.hpp"
#include <iostream>

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
}

Image::~Image() {
    auto it = images.find(imageId);
    if (it != images.end()) {
        freeImage(imageId);
    }
}

void Image::render(double xPos, double yPos, bool centered) {
    if (images.find(imageId) != images.end()) {
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
}

/**
 * Takes every image in a Scratch sb3 file and turns it into an 'SDL_Image' object.
 * @param zip
 */
void Image::loadImages(mz_zip_archive *zip) {
    // Log::log("Loading images...");
    // int file_count = (int)mz_zip_reader_get_num_files(zip);

    // for (int i = 0; i < file_count; i++) {
    //     mz_zip_archive_file_stat file_stat;
    //     if (!mz_zip_reader_file_stat(zip, i, &file_stat)) continue;

    //     std::string zipFileName = file_stat.m_filename;

    //     // Check if file is bitmap, or SVG
    //     bool isBitmap = zipFileName.size() >= 4 &&
    //                     (zipFileName.substr(zipFileName.size() - 4) == ".png" ||
    //                      zipFileName.substr(zipFileName.size() - 4) == ".PNG" ||
    //                      zipFileName.substr(zipFileName.size() - 4) == ".jpg" ||
    //                      zipFileName.substr(zipFileName.size() - 4) == ".JPG");
    //     bool isSVG = zipFileName.size() >= 4 &&
    //                  (zipFileName.substr(zipFileName.size() - 4) == ".svg" ||
    //                   zipFileName.substr(zipFileName.size() - 4) == ".SVG");

    //     if (isBitmap || isSVG) {

    //         size_t file_size;
    //         void *file_data = mz_zip_reader_extract_to_heap(zip, i, &file_size, 0);
    //         if (!file_data) {
    //             Log::logWarning("Failed to extract: " + zipFileName);
    //             continue;
    //         }

    //         // Use SDL_RWops to load image from memory
    //         SDL_RWops *rw = SDL_RWFromMem(file_data, file_size);
    //         if (!rw) {
    //             Log::logWarning("Failed to create RWops for: " + zipFileName);
    //             mz_free(file_data);
    //             continue;
    //         }

    //         SDL_Surface *surface = IMG_Load_RW(rw, 0);
    //         SDL_RWclose(rw);
    //         mz_free(file_data);

    //         if (!surface) {
    //             Log::logWarning("Failed to load image from memory: " + zipFileName);
    //             continue;
    //         }

    //         SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    //         if (!texture) {
    //             Log::logWarning("Failed to create texture: " + zipFileName);
    //             SDL_FreeSurface(surface);
    //             continue;
    //         }

    //         // Track texture memory usage
    //         int width, height;
    //         SDL_QueryTexture(texture, nullptr, nullptr, &width, &height);
    //         size_t textureMemory = width * height * 4;
    //         MemoryTracker::allocate(textureMemory);

    //         SDL_FreeSurface(surface);

    //         // Build SDL_Image object
    //         SDL_Image *image = MemoryTracker::allocate<SDL_Image>();
    //         new (image) SDL_Image();
    //         if (isSVG) image->isSVG = true;
    //         image->spriteTexture = texture;
    //         SDL_QueryTexture(texture, nullptr, nullptr, &image->width, &image->height);
    //         image->renderRect = {0, 0, image->width, image->height};
    //         image->textureRect = {0, 0, image->width, image->height};

    //         image->memorySize = textureMemory;

    //         // Strip extension from filename for the ID
    //         std::string imgId = zipFileName.substr(0, zipFileName.find_last_of('.'));
    //         images[imgId] = image;
    //     }
    // }
}

/**
 * Loads a single `SDL_Image` from an unzipped filepath .
 * @param filePath
 */
bool Image::loadImageFromFile(std::string filePath, bool fromScratchProject) {
    std::string imgId = filePath.substr(0, filePath.find_last_of('.'));
    if (images.find(imgId) != images.end()) return true;

    std::string finalPath;

#if defined(__WIIU__) || defined(__OGC__)
    finalPath = "romfs:/";
#endif
    if (fromScratchProject)
        finalPath = finalPath + "project/";

    finalPath = finalPath + filePath;

    // SDL_Image *image = new SDL_Image(finalPath);
    SDL_Image *image = MemoryTracker::allocate<SDL_Image>();
    new (image) SDL_Image(finalPath);

    // Check if it's an SVG file
    bool isSVG = filePath.size() >= 4 &&
                 (filePath.substr(filePath.size() - 4) == ".svg" ||
                  filePath.substr(filePath.size() - 4) == ".SVG");

    if (isSVG) image->isSVG = true;

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

    Log::log("Loading single image: " + costumeId);

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
    bool isBitmap = costumeId.size() >= 4 &&
                    (costumeId.substr(costumeId.size() - 4) == ".png" ||
                     costumeId.substr(costumeId.size() - 4) == ".PNG" ||
                     costumeId.substr(costumeId.size() - 4) == ".jpg" ||
                     costumeId.substr(costumeId.size() - 4) == ".JPG");
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
    if (isSVG) image->isSVG = true;
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

    Log::log("Successfully loaded image: " + costumeId);
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

        Log::log("Freed image " + costumeId);
        // Call destructor and deallocate SDL_Image
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

    Log::log("Image loaded!");
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