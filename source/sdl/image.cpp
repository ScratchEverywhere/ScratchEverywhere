#include "../scratch/image.hpp"
#include "../scratch/os.hpp"
#include "image.hpp"
#include "render.hpp"
#include <iostream>

std::vector<Image::ImageRGBA> Image::imageRGBAS;
std::unordered_map<std::string, SDL_Image *> images;

/**
 * Takes every image in a Scratch sb3 file and turns it into an 'SDL_Image' object.
 * @param zip
 */
void Image::loadImages(mz_zip_archive *zip) {
    Log::log("Loading images...");
    int file_count = (int)mz_zip_reader_get_num_files(zip);

    for (int i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip, i, &file_stat)) continue;

        std::string zipFileName = file_stat.m_filename;

        // Check if file is PNG or JPG
        if (zipFileName.size() >= 4 &&
            (zipFileName.substr(zipFileName.size() - 4) == ".png" || zipFileName.substr(zipFileName.size() - 4) == ".PNG" || zipFileName.substr(zipFileName.size() - 4) == ".jpg" || zipFileName.substr(zipFileName.size() - 4) == ".JPG")) {

            size_t file_size;
            void *file_data = mz_zip_reader_extract_to_heap(zip, i, &file_size, 0);
            if (!file_data) {
                Log::logWarning("Failed to extract: " + zipFileName);
                continue;
            }

            // Use SDL_RWops to load image from memory
            SDL_RWops *rw = SDL_RWFromMem(file_data, file_size);
            if (!rw) {
                Log::logWarning("Failed to create RWops for: " + zipFileName);
                mz_free(file_data);
                continue;
            }

            SDL_Surface *surface = IMG_Load_RW(rw, 0);
            SDL_RWclose(rw);
            mz_free(file_data);

            if (!surface) {
                Log::logWarning("Failed to load image from memory: " + zipFileName);
                continue;
            }

            SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (!texture) {
                Log::logWarning("Failed to create texture: " + zipFileName);
                SDL_FreeSurface(surface);
                continue;
            }

            // Track texture memory usage
            int width, height;
            SDL_QueryTexture(texture, nullptr, nullptr, &width, &height);
            size_t textureMemory = width * height * 4;
            MemoryTracker::allocate(textureMemory);

            SDL_FreeSurface(surface);

            // Build SDL_Image object
            SDL_Image *image = MemoryTracker::allocate<SDL_Image>();
            new (image) SDL_Image();
            image->spriteTexture = texture;
            SDL_QueryTexture(texture, nullptr, nullptr, &image->width, &image->height);
            image->renderRect = {0, 0, image->width, image->height};
            image->textureRect = {0, 0, image->width, image->height};

            image->memorySize = textureMemory;

            // Strip extension from filename for the ID
            std::string imageId = zipFileName.substr(0, zipFileName.find_last_of('.'));
            images[imageId] = image;
        }
    }
}

/**
 * Loads a single `SDL_Image` from an unzipped filepath .
 * @param filePath
 */
void Image::loadImageFromFile(std::string filePath) {
    SDL_Image *image = MemoryTracker::allocate<SDL_Image>();
    new (image) SDL_Image("project/" + filePath);

    // Track texture memory
    if (image->spriteTexture) {
        size_t textureMemory = image->width * image->height * 4;
        MemoryTracker::allocate(textureMemory);
        image->memorySize = textureMemory;
    }

    std::string imageId = filePath.substr(0, filePath.find_last_of('.'));
    images[imageId] = image;
}
/**
 * Frees an `SDL_Image` from memory using a `costumeId` to find it.
 * @param costumeId
 */
void Image::freeImage(const std::string &costumeId) {
    auto imageIt = images.find(costumeId);
    if (imageIt != images.end()) {
        SDL_Image *image = imageIt->second;

        if (image->memorySize > 0) {
            MemoryTracker::deallocate(nullptr, image->memorySize);
        }

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
    std::vector<std::string> toDelete;

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

    memorySize = width * height * 4;

    Log::log("Image loaded!");
}

/**
 * currently does nothing in the SDL version 😁😁
 */
void Image::queueFreeImage(const std::string &costumeId) {
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