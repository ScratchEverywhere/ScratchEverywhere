#include "image_sdl2.hpp"
#include "render.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>

void Image_SDL2::render(ImageRenderParams &params) {

    const int &x = params.x;
    const int &y = params.y;
    const int &brightness = params.brightness;
    const double rotation = Math::radiansToDegrees(params.rotation);
    const float &scale = params.scale;
    const float &opacity = params.opacity;
    const bool &centered = params.centered;
    SDL_RendererFlip flip = params.flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    SDL_Rect renderRect;
    renderRect.w = static_cast<int>(imgData.width * scale);
    renderRect.h = static_cast<int>(imgData.height * scale);

    SDL_Rect subRect;
    if (params.subrect != nullptr) {
        subRect = {
            .x = params.subrect->x,
            .y = params.subrect->y,
            .w = params.subrect->w,
            .h = params.subrect->h};
    } else {
        subRect = {
            .x = 0,
            .y = 0,
            .w = imgData.width,
            .h = imgData.height};
    }

    if (centered) {
        renderRect.x = x - (renderRect.w / 2);
        renderRect.y = y - (renderRect.h / 2);
    } else {
        renderRect.x = x;
        renderRect.y = y;
    }

    Uint8 alpha = static_cast<Uint8>(opacity * 255);
    SDL_SetTextureAlphaMod(texture, alpha);

    const SDL_Point center = {renderRect.w / 2, renderRect.h / 2};

    if (brightness != 0) {
        float b = brightness * 0.01f;
        if (brightness > 0.0f) {
            // render the normal image first
            SDL_RenderCopyEx(renderer, texture, &subRect, &renderRect, rotation, &center, flip);

            // render another, blended image on top
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_ADD);
            SDL_SetTextureAlphaMod(texture, Uint8(255 * b * opacity));
            SDL_RenderCopyEx(renderer, texture, &subRect, &renderRect, rotation, &center, flip);

            // reset for next frame
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        } else {
            Uint8 col = Uint8(255 * std::clamp(1.0f + b, 0.0f, 1.0f));
            SDL_SetTextureColorMod(texture, col, col, col);

            SDL_RenderCopyEx(renderer, texture, &subRect, &renderRect, rotation, &center, flip);
            // reset for next frame
            SDL_SetTextureColorMod(texture, 255, 255, 255);
        }
    } else {
        // if no brightness just render normal image
        SDL_SetTextureColorMod(texture, 255, 255, 255);
        SDL_RenderCopyEx(renderer, texture, &subRect, &renderRect, rotation, &center, flip);
    }
    freeTimer = maxFreeTimer;
}

// I doubt you want to mess with this...
void Image_SDL2::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {

    const int iDestX = static_cast<int>(xPos - (centered ? width / 2 : 0));
    const int iDestY = static_cast<int>(yPos - (centered ? height / 2 : 0));
    const int iWidth = static_cast<int>(width);
    const int iHeight = static_cast<int>(height);
    const int iSrcPadding = std::max(1, static_cast<int>(std::min(std::min(padding, static_cast<double>(getWidth()) / 2), static_cast<double>(getHeight()) / 2)));

    const int srcCenterWidth = std::max(0, getWidth() - 2 * iSrcPadding);
    const int srcCenterHeight = std::max(0, getHeight() - 2 * iSrcPadding);

    const SDL_Rect srcTopLeft = {0, 0, iSrcPadding, iSrcPadding};
    const SDL_Rect srcTop = {iSrcPadding, 0, srcCenterWidth, iSrcPadding};
    const SDL_Rect srcTopRight = {getWidth() - iSrcPadding, 0, iSrcPadding, iSrcPadding};
    const SDL_Rect srcLeft = {0, iSrcPadding, iSrcPadding, srcCenterHeight};
    const SDL_Rect srcCenter = {iSrcPadding, iSrcPadding, srcCenterWidth, srcCenterHeight};
    const SDL_Rect srcRight = {getWidth() - iSrcPadding, iSrcPadding, iSrcPadding, srcCenterHeight};
    const SDL_Rect srcBottomLeft = {0, getHeight() - iSrcPadding, iSrcPadding, iSrcPadding};
    const SDL_Rect srcBottom = {iSrcPadding, getHeight() - iSrcPadding, srcCenterWidth, iSrcPadding};
    const SDL_Rect srcBottomRight = {getWidth() - iSrcPadding, getHeight() - iSrcPadding, iSrcPadding, iSrcPadding};

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
    freeTimer = maxFreeTimer;
}

void Image_SDL2::setInitialTexture() {
    SDL_PixelFormatEnum format;
#ifdef __PS4__
    format = SDL_PIXELFORMAT_RGBA8888;
#else
    format = SDL_PIXELFORMAT_RGBA32;
#endif

    texture = SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_STATIC, imgData.width, imgData.height);

    if (!texture) {
        throw std::runtime_error("Failed to create texture: " + std::string(SDL_GetError()));
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    if (SDL_UpdateTexture(texture, nullptr, imgData.pixels, imgData.pitch) < 0) {
        throw std::runtime_error("Failed to update texture: " + std::string(SDL_GetError()));
    }

    /** some platforms may need this to be freed due to RAM limits,
     *  but they then wont be able to support Image::getPixels()
     *  */
    // free(imgData.pixels);
    // pixels = nullptr;
}

Image_SDL2::Image_SDL2(std::string filePath, mz_zip_archive *zip) : Image(filePath, zip) {
    setInitialTexture();
}

Image_SDL2::Image_SDL2(std::string filePath, bool fromScratchProject) : Image(filePath, fromScratchProject) {
    setInitialTexture();
}

Image_SDL2::~Image_SDL2() {
    SDL_DestroyTexture(texture);
}