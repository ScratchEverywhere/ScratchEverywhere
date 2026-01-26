#include "image_sdl2.hpp"
#include "render.hpp"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <miniz.h>
#include <os.hpp>
#include <string>
#include <unordered_map>
#include <unzip.hpp>
#include <vector>

static std::vector<std::string> toDelete;

#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

// Image::Image(std::string filePath) {
//     if (!loadImageFromFile(filePath, nullptr, false)) return;
//     std::string imgId = filePath.substr(0, filePath.find_last_of('.'));
//     imageId = imgId;
//     width = images[imgId]->width;
//     height = images[imgId]->height;
//     scale = 1.0;
//     rotation = 0.0;
//     opacity = 1.0;
//     images[imgId]->imageUsageCount++;
// }

// Image::~Image() {
//     auto it = images.find(imageId);
//     if (it != images.end()) {
//         images[imageId]->imageUsageCount--;
//         if (images[imageId]->imageUsageCount <= 0)
//             freeImage(imageId);
//     }
// }

void Image_SDL2::render(ImageRenderParams &params) {

    int &x = params.x;
    int &y = params.y;
    int &brightness = params.brightness;
    float &rotation = params.rotation;
    float &scale = params.scale;
    float &opacity = params.opacity;
    bool &centered = params.centered;
    SDL_RendererFlip flip = params.flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    SDL_Rect renderRect;
    renderRect.w = width;
    renderRect.h = height;

    if (centered) {
        renderRect.x = x - (renderRect.w / 2);
        renderRect.y = y - (renderRect.h / 2);
    } else {
        renderRect.x = x;
        renderRect.y = y;
    }

    Uint8 alpha = static_cast<Uint8>(opacity * 255);
    SDL_SetTextureAlphaMod(texture, alpha);

    SDL_Point center = {renderRect.w / 2, renderRect.h / 2};

    if (brightness != 0) {
        float b = brightness * 0.01f;
        if (brightness > 0.0f) {
            // render the normal image first
            SDL_RenderCopyEx(renderer, texture, NULL, &renderRect, rotation, &center, flip);

            // render another, blended image on top
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_ADD);
            SDL_SetTextureAlphaMod(texture, (Uint8)(brightness * 255 * (alpha / 255.0f)));
            SDL_RenderCopyEx(renderer, texture, NULL, &renderRect, rotation, &center, flip);

            // reset for next frame
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        } else {
            Uint8 col = static_cast<Uint8>(255 * (1.0f + brightness));
            SDL_SetTextureColorMod(texture, col, col, col);

            SDL_RenderCopyEx(renderer, texture, NULL, &renderRect, rotation, &center, flip);
            // reset for next frame
            SDL_SetTextureColorMod(texture, 255, 255, 255);
        }
    } else {
        // if no brightness just render normal image
        SDL_SetTextureColorMod(texture, 255, 255, 255);
        SDL_RenderCopyEx(renderer, texture, NULL, &renderRect, rotation, &center, flip);
    }
}

// I doubt you want to mess with this...
void Image_SDL2::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
    // if (images.find(imageId) == images.end()) return;
    // SDL_Image *image = images[imageId];

    // image->setScale(1.0);
    // image->setRotation(0.0);

    // uint8_t alpha = static_cast<Uint8>(opacity * 255);
    // SDL_SetTextureAlphaMod(image->spriteTexture, alpha);

    const int iDestX = static_cast<int>(xPos - (centered ? width / 2 : 0));
    const int iDestY = static_cast<int>(yPos - (centered ? height / 2 : 0));
    const int iWidth = static_cast<int>(width);
    const int iHeight = static_cast<int>(height);
    const int iSrcPadding = std::max(1, static_cast<int>(std::min(std::min(padding, static_cast<double>(width) / 2), static_cast<double>(height) / 2)));

    const int srcCenterWidth = std::max(0.0, width - 2 * iSrcPadding);
    const int srcCenterHeight = std::max(0.0, height - 2 * iSrcPadding);

    const SDL_Rect srcTopLeft = {0, 0, iSrcPadding, iSrcPadding};
    const SDL_Rect srcTop = {iSrcPadding, 0, srcCenterWidth, iSrcPadding};
    const SDL_Rect srcTopRight = {iWidth - iSrcPadding, 0, iSrcPadding, iSrcPadding};
    const SDL_Rect srcLeft = {0, iSrcPadding, iSrcPadding, srcCenterHeight};
    const SDL_Rect srcCenter = {iSrcPadding, iSrcPadding, srcCenterWidth, srcCenterHeight};
    const SDL_Rect srcRight = {iWidth - iSrcPadding, iSrcPadding, iSrcPadding, srcCenterHeight};
    const SDL_Rect srcBottomLeft = {0, iHeight - iSrcPadding, iSrcPadding, iSrcPadding};
    const SDL_Rect srcBottom = {iSrcPadding, iHeight - iSrcPadding, srcCenterWidth, iSrcPadding};
    const SDL_Rect srcBottomRight = {iWidth - iSrcPadding, iHeight - iSrcPadding, iSrcPadding, iSrcPadding};

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

    SDL_Texture *originalTexture = texture;
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

// /**
//  * Loads a single `SDL_Image` from an unzipped filepath .
//  * @param filePath
//  */
// bool Image::loadImageFromFile(std::string filePath, Sprite *sprite, bool fromScratchProject) {
//     std::string imgId = filePath.substr(0, filePath.find_last_of('.'));
//     if (images.find(imgId) != images.end()) return true;

//     std::string finalPath;

//     finalPath = OS::getRomFSLocation();
//     if (fromScratchProject)
//         finalPath = finalPath + "project/";

//     finalPath = finalPath + filePath;
//     if (Unzip::UnpackedInSD && fromScratchProject) finalPath = Unzip::filePath + filePath;
//     SDL_Image *image = new SDL_Image(finalPath, fromScratchProject);

//     if (!image->spriteTexture || image->spriteTexture == NULL ||
//         !image->spriteSurface || image->spriteSurface == NULL) {
//         return false;
//     }

//     if (sprite != nullptr) {
//         sprite->spriteWidth = image->textureRect.w / 2;
//         sprite->spriteHeight = image->textureRect.h / 2;
//     }

//     images[imgId] = image;
//     return true;
// }

// /**
//  * Loads a single image from a Scratch sb3 zip file by filename.
//  * @param zip Pointer to the zip archive
//  * @param costumeId The filename of the image to load (e.g., "sprite1.png")
//  */
// void Image::loadImageFromSB3(mz_zip_archive *zip, const std::string &costumeId, Sprite *sprite) {
//     std::string imgId = costumeId.substr(0, costumeId.find_last_of('.'));
//     if (images.find(imgId) != images.end()) return;

//     // Log::log("Loading single image: " + costumeId);

//     // Find the file in the zip
//     int file_index = mz_zip_reader_locate_file(zip, costumeId.c_str(), nullptr, 0);
//     if (file_index < 0) {
//         Log::logWarning("Image file not found in zip: " + costumeId);
//         return;
//     }

//     // Get file stats
//     mz_zip_archive_file_stat file_stat;
//     if (!mz_zip_reader_file_stat(zip, file_index, &file_stat)) {
//         Log::logWarning("Failed to get file stats for: " + costumeId);
//         return;
//     }

//     // Check if file is bitmap or SVG
//     bool isSupported = costumeId.size() > 4 && ([](std::string ext) {
//                            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
//                            return ext == ".bmp" || ext == ".gif" || ext == ".jpg" || ext == ".jpeg" ||
//                                   ext == ".lbm" || ext == ".iff" || ext == ".pcx" || ext == ".png" ||
//                                   ext == ".pnm" || ext == ".ppm" || ext == ".pgm" || ext == ".pbm" ||
//                                   ext == ".qoi" || ext == ".tga" || ext == ".tiff" || ext == ".xcf" ||
//                                   ext == ".xpm" || ext == ".xv" || ext == ".ico" || ext == ".cur" ||
//                                   ext == ".ani" || ext == ".webp" || ext == ".avif" || ext == ".jxl" ||
//                                   ext == ".svg";
//                        }(costumeId.substr(costumeId.find_last_of('.'))));

//     if (!isSupported) {
//         Log::logWarning("File is not a supported image format: " + costumeId);
//         return;
//     }

//     // Extract file data
//     size_t file_size;
//     void *file_data = mz_zip_reader_extract_to_heap(zip, file_index, &file_size, 0);
//     if (!file_data) {
//         Log::logWarning("Failed to extract: " + costumeId);
//         return;
//     }

//     // Use SDL_RWops to load image from memory
//     SDL_RWops *rw = SDL_RWFromMem(file_data, file_size);
//     if (!rw) {
//         Log::logWarning("Failed to create RWops for: " + costumeId);
//         mz_free(file_data);
//         return;
//     }

//     SDL_Surface *surface = IMG_Load_RW(rw, 0);
//     SDL_RWclose(rw);
//     mz_free(file_data);

//     if (!surface) {
//         const std::string error = std::string(IMG_GetError());
//         Log::logWarning("Failed to load image from memory: " + costumeId);
//         Log::logWarning("IMG Error: " + error);
//         if (error.find("Out of memory") != std::string::npos) Image::cleanupImagesLite();
//         return;
//     }

// // PS4 piglet expects RGBA instead of ABGR.
// #if defined(__PS4__)
//     SDL_Surface *convert = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888, 0);
//     if (convert == NULL) {
//         Log::logWarning(std::string("Error converting image surface: ") + SDL_GetError());
//         SDL_FreeSurface(convert);
//         return;
//     }

//     SDL_FreeSurface(surface);
//     surface = convert;
// #endif

//     SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
//     if (!texture) {
//         Log::logWarning("Failed to create texture: " + costumeId);
//         SDL_FreeSurface(surface);
//         return;
//     }
//     SDL_FreeSurface(surface);

//     // Build SDL_Image object
//     SDL_Image *image = new SDL_Image();
//     image->spriteTexture = texture;
//     SDL_QueryTexture(texture, nullptr, nullptr, &image->width, &image->height);
//     image->renderRect = {0, 0, image->width, image->height};
//     image->textureRect = {0, 0, image->width, image->height};

//     if (sprite != nullptr) {
//         sprite->spriteWidth = image->textureRect.w / 2;
//         sprite->spriteHeight = image->textureRect.h / 2;
//     }

//     // Log::log("Successfully loaded image: " + costumeId);
//     images[imgId] = image;
// }

// void Image::cleanupImages() {
//     for (auto &[id, image] : images) {
//         delete image;
//     }
//     images.clear();
//     toDelete.clear();
// }

// void Image::cleanupImagesLite() {
//     std::vector<std::string> keysToDelete;
//     keysToDelete.reserve(images.size());

//     for (const auto &[id, data] : images) {
//         if (data->freeTimer < data->maxFreeTime - 2)
//             keysToDelete.push_back(id);
//     }

//     for (const std::string &id : keysToDelete) {
//         Image::freeImage(id);
//     }
// }

// /**
//  * Frees an `SDL_Image` from memory using a `costumeId` to find it.
//  * @param costumeId
//  */
// void Image::freeImage(const std::string &costumeId) {
//     auto imageIt = images.find(costumeId);
//     if (imageIt != images.end()) {
//         SDL_Image *image = imageIt->second;
//         delete image;
//         images.erase(imageIt);
//     }
// }

// /**
//  * Checks every `SDL_Image` in memory to see if they can be freed.
//  * An `SDL_Image` will get freed if it goes unused for 120 frames.
//  */
// void Image::FlushImages() {

//     // Free images based on a timer
//     for (auto &[id, img] : images) {
//         if (img->freeTimer <= 0) {
//             toDelete.push_back(id);
//         } else {
//             img->freeTimer -= 1;
//         }
//     }

//     for (const std::string &id : toDelete) {
//         Image::freeImage(id);
//     }
//     toDelete.clear();
// }

Image_SDL2::Image_SDL2(std::string filePath, mz_zip_archive *zip) : Image(filePath, zip) {
}

Image_SDL2::Image_SDL2(std::string filePath, bool fromScratchProject) : Image(filePath, fromScratchProject) {
    // #ifdef USE_CMAKERC
    //     if (!Unzip::UnpackedInSD || !fromScratchProject) {
    //         const auto &file = cmrc::romfs::get_filesystem().open(filePath);
    //         spriteSurface = IMG_Load_RW(SDL_RWFromConstMem(file.begin(), file.size()), 1);
    //     } else spriteSurface = IMG_Load(filePath.c_str());

    // #else
    //     spriteSurface = IMG_Load(filePath.c_str());
    // #endif
    //     if (spriteSurface == NULL) {
    //         const std::string error = std::string(IMG_GetError());
    //         Log::logWarning("Error loading image '" + filePath + "': " + error);
    //         if (error.find("Out of memory") != std::string::npos) Image::cleanupImagesLite();
    //         return;
    //     }

    // // PS4 piglet expects RGBA instead of ABGR.
    // #if defined(__PS4__)
    //     SDL_Surface *convert = SDL_ConvertSurfaceFormat(spriteSurface, SDL_PIXELFORMAT_RGBA8888, 0);
    //     if (convert == NULL) {
    //         Log::logWarning(std::string("Error converting image surface: ") + SDL_GetError());
    //         SDL_FreeSurface(convert);
    //         return;
    //     }

    //     SDL_FreeSurface(spriteSurface);
    //     spriteSurface = convert;
    // #endif

    //     spriteTexture = SDL_CreateTextureFromSurface(renderer, spriteSurface);
    //     if (spriteTexture == NULL) {
    //         Log::logWarning(std::string("Error creating texture: ") + SDL_GetError());
    //         return;
    //     }
    //     SDL_FreeSurface(spriteSurface);

    //     // get width and height of image
    //     int texW = 0;
    //     int texH = 0;
    //     SDL_QueryTexture(spriteTexture, NULL, NULL, &texW, &texH);
    //     width = texW;
    //     height = texH;
    //     renderRect.w = width;
    //     renderRect.h = height;
    //     textureRect.w = width;
    //     textureRect.h = height;
    //     textureRect.x = 0;
    //     textureRect.y = 0;

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);

    if (!texture) {
        throw std::runtime_error("Failed to create texture: " + std::string(SDL_GetError()));
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    if (SDL_UpdateTexture(texture, nullptr, pixels, width * 4) < 0) {
        throw std::runtime_error("Failed to update texture: " + std::string(SDL_GetError()));
    }
    free(pixels);
    pixels = nullptr;
}

Image_SDL2::~Image_SDL2() {
    SDL_DestroyTexture(texture);
}
