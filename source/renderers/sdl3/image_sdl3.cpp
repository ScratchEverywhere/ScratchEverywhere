#include "image_sdl3.hpp"
#include "render.hpp"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <image.hpp>
#include <miniz.h>
#include <os.hpp>
#include <string>
#include <unordered_map>
#include <unzip.hpp>
#include <vector>

void Image_SDL3::render(ImageRenderParams &params) {

    const int &x = params.x;
    const int &y = params.y;
    const int &brightness = params.brightness;
    const double rotation = Math::radiansToDegrees(params.rotation);
    const float &scale = params.scale;
    const float &opacity = params.opacity;
    const bool &centered = params.centered;
    SDL_FlipMode flip = params.flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    SDL_FRect renderRect;
    renderRect.w = static_cast<int>(imgData.width / imgData.scale * scale);
    renderRect.h = static_cast<int>(imgData.height / imgData.scale * scale);

    SDL_FRect subRect;
    if (params.subrect != nullptr) {
        subRect = {
            .x = static_cast<float>(params.subrect->x),
            .y = static_cast<float>(params.subrect->y),
            .w = static_cast<float>(params.subrect->w),
            .h = static_cast<float>(params.subrect->h)};
    } else {
        subRect = {
            .x = 0.0f,
            .y = 0.0f,
            .w = static_cast<float>(imgData.width),
            .h = static_cast<float>(imgData.height)};
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

    const SDL_FPoint center = {renderRect.w / 2.0f, renderRect.h / 2.0f};

    if (brightness != 0) {
        float b = brightness * 0.01f;
        if (brightness > 0.0f) {
            // render the normal image first
            SDL_RenderTextureRotated(renderer, texture, &subRect, &renderRect, rotation, &center, flip);

            // render another, blended image on top
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_ADD);
            SDL_SetTextureAlphaMod(texture, Uint8(255 * b * opacity));
            SDL_RenderTextureRotated(renderer, texture, &subRect, &renderRect, rotation, &center, flip);

            // reset for next frame
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        } else {
            Uint8 col = Uint8(255 * std::clamp(1.0f + b, 0.0f, 1.0f));
            SDL_SetTextureColorMod(texture, col, col, col);

            SDL_RenderTextureRotated(renderer, texture, &subRect, &renderRect, rotation, &center, flip);
            // reset for next frame
            SDL_SetTextureColorMod(texture, 255, 255, 255);
        }
    } else {
        // if no brightness just render normal image
        SDL_SetTextureColorMod(texture, 255, 255, 255);
        SDL_RenderTextureRotated(renderer, texture, &subRect, &renderRect, rotation, &center, flip);
    }
    freeTimer = maxFreeTimer;
}

// I doubt you want to mess with this...
void Image_SDL3::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
    const float destX = xPos - (centered ? width / 2.0f : 0);
    const float destY = yPos - (centered ? height / 2.0f : 0);
    const float srcPadding = std::max(1.0f, std::min(std::min(static_cast<float>(padding), getWidth() / 2.0f), static_cast<float>(getHeight())) / 2.0f);

    const float srcCenterWidth = std::max(0.0f, imgData.width - 2 * srcPadding);
    const float srcCenterHeight = std::max(0.0f, imgData.height - 2 * srcPadding);

    const SDL_FRect srcTopLeft = {0, 0, srcPadding, srcPadding};
    const SDL_FRect srcTop = {srcPadding, 0, srcCenterWidth, srcPadding};
    const SDL_FRect srcTopRight = {imgData.width - srcPadding, 0, srcPadding, srcPadding};
    const SDL_FRect srcLeft = {0, srcPadding, srcPadding, srcCenterHeight};
    const SDL_FRect srcCenter = {srcPadding, srcPadding, srcCenterWidth, srcCenterHeight};
    const SDL_FRect srcRight = {imgData.width - srcPadding, srcPadding, srcPadding, srcCenterHeight};
    const SDL_FRect srcBottomLeft = {0, imgData.height - srcPadding, srcPadding, srcPadding};
    const SDL_FRect srcBottom = {srcPadding, imgData.height - srcPadding, srcCenterWidth, srcPadding};
    const SDL_FRect srcBottomRight = {imgData.width - srcPadding, imgData.height - srcPadding, srcPadding, srcPadding};

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

    SDL_ScaleMode originalScaleMode;
    SDL_GetTextureScaleMode(texture, &originalScaleMode);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    SDL_RenderTexture(renderer, texture, &srcTopLeft, &dstTopLeft);
    SDL_RenderTexture(renderer, texture, &srcTop, &dstTop);
    SDL_RenderTexture(renderer, texture, &srcTopRight, &dstTopRight);
    SDL_RenderTexture(renderer, texture, &srcLeft, &dstLeft);
    SDL_RenderTexture(renderer, texture, &srcCenter, &dstCenter);
    SDL_RenderTexture(renderer, texture, &srcRight, &dstRight);
    SDL_RenderTexture(renderer, texture, &srcBottomLeft, &dstBottomLeft);
    SDL_RenderTexture(renderer, texture, &srcBottom, &dstBottom);
    SDL_RenderTexture(renderer, texture, &srcBottomRight, &dstBottomRight);

    SDL_SetTextureScaleMode(texture, originalScaleMode);
    freeTimer = maxFreeTimer;
}

void *Image_SDL3::getNativeTexture() {
    return texture;
}

void Image_SDL3::setInitialTexture() {
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, imgData.width, imgData.height);

    if (!texture) {
        throw std::runtime_error("Failed to create texture: " + std::string(SDL_GetError()));
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    if (!SDL_UpdateTexture(texture, nullptr, imgData.pixels, imgData.pitch)) {
        throw std::runtime_error("Failed to update texture: " + std::string(SDL_GetError()));
    }
}

void Image_SDL3::refreshTexture() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    setInitialTexture();
}

Image_SDL3::Image_SDL3(std::string filePath, mz_zip_archive *zip, bool bitmapHalfQuality) : Image(filePath, zip, bitmapHalfQuality) {
    setInitialTexture();
}

Image_SDL3::Image_SDL3(std::string filePath, bool fromScratchProject, bool bitmapHalfQuality) : Image(filePath, fromScratchProject, bitmapHalfQuality) {
    setInitialTexture();
}

Image_SDL3::~Image_SDL3() {
    SDL_DestroyTexture(texture);
}
