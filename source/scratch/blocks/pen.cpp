#include "pen.hpp"
#include "../interpret.hpp"
#include "../render.hpp"

#ifdef __3DS__
#include "../../3ds/image.hpp"
#include <citro2d.h>
#include <citro3d.h>
C2D_Image penImage;
C3D_RenderTarget *penRenderTarget;
Tex3DS_SubTexture penSubtex;
C3D_Tex *penTex;
#elif defined(SDL_BUILD)
#include "../../sdl/image.hpp"
#include "../../sdl/render.hpp"
#include <SDL2/SDL2_gfxPrimitives.h>

SDL_Texture *penTexture;
#else
#error Unsupported Platform.
#endif

const unsigned int minPenSize = 1;
const unsigned int maxPenSize = 1000;

BlockResult PenBlocks::PenDown(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!Render::initPen()) return BlockResult::CONTINUE;
    sprite->penData.down = true;

#ifdef SDL_BUILD
    SDL_Texture *tempTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, Scratch::projectWidth, Scratch::projectHeight);
    SDL_SetTextureBlendMode(tempTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(tempTexture, (sprite->penData.transparency - 100) / 100 * 255);
    SDL_SetRenderTarget(renderer, tempTexture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    const ColorRGB rgbColor = HSB2RGB(sprite->penData.color);
    filledCircleRGBA(renderer, sprite->xPosition + Scratch::projectWidth / 2, -sprite->yPosition + Scratch::projectHeight / 2, sprite->penData.size / 2, rgbColor.r, rgbColor.g, rgbColor.b, 255);

    SDL_SetRenderTarget(renderer, penTexture);
    SDL_RenderCopy(renderer, tempTexture, NULL, NULL);
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_DestroyTexture(tempTexture);
#elif defined(__3DS__)
    const ColorRGB rgbColor = HSB2RGB(sprite->penData.color);
    const int transparency = 255 * (1 - sprite->penData.transparency / 100);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C3D_FrameDrawOn(penRenderTarget);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

    const int SCREEN_WIDTH = 400;
    const int SCREEN_HEIGHT = 240;

    const float scaleX = static_cast<double>(SCREEN_WIDTH) / penSubtex.width;
    const float scaleY = static_cast<double>(SCREEN_HEIGHT) / penSubtex.height;
    const float scale = std::min(scaleX, scaleY);
    const u32 color = C2D_Color32(rgbColor.r, rgbColor.g, rgbColor.b, transparency);
    const int thickness = std::clamp(static_cast<int>(sprite->penData.size * scale), 1, 1000);

    const float xSscaled = (sprite->xPosition * scale) + (SCREEN_WIDTH / 2);
    const float yScaled = (sprite->yPosition * -1 * scale) + (SCREEN_HEIGHT * 0.5);
    const float radius = thickness / 2.0f;
    C2D_DrawCircleSolid(xSscaled, yScaled, 0, radius, color);
#endif

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::PenUp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.down = false;

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::SetPenOptionTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {

    Block *optionBlock = findBlock(Scratch::getInputValue(block, "COLOR_PARAM", sprite).asString());

    if (optionBlock != nullptr) {

        const std::string option = Scratch::getFieldValue(*optionBlock, "colorParam");

        if (option == "color") {
            sprite->penData.color.hue = Scratch::getInputValue(block, "VALUE", sprite).asInt() % 100;
            return BlockResult::CONTINUE;
        }
        if (option == "saturation") {
            sprite->penData.color.saturation = Scratch::getInputValue(block, "VALUE", sprite).asDouble();
            if (sprite->penData.color.saturation < 0) sprite->penData.color.saturation = 0;
            else if (sprite->penData.color.saturation > 100) sprite->penData.color.saturation = 100;
            return BlockResult::CONTINUE;
        }
        if (option == "brightness") {
            sprite->penData.color.brightness = Scratch::getInputValue(block, "VALUE", sprite).asDouble();
            if (sprite->penData.color.brightness < 0) sprite->penData.color.brightness = 0;
            else if (sprite->penData.color.brightness > 100) sprite->penData.color.brightness = 100;
            return BlockResult::CONTINUE;
        }
        if (option == "transparency") {
            sprite->penData.transparency = Scratch::getInputValue(block, "VALUE", sprite).asDouble();
            if (sprite->penData.transparency < 0) sprite->penData.transparency = 0;
            else if (sprite->penData.transparency > 100) sprite->penData.transparency = 100;
            return BlockResult::CONTINUE;
        }
    }

    Log::log("Unknown pen option!");

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::ChangePenOptionBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {

    Block *optionBlock = findBlock(Scratch::getInputValue(block, "COLOR_PARAM", sprite).asString());

    if (optionBlock != nullptr) {

        const std::string option = Scratch::getFieldValue(block, "COLOR_PARAM");

        if (option == "color") {
            sprite->penData.color.hue += Scratch::getInputValue(block, "VALUE", sprite).asInt() % 100;
            return BlockResult::CONTINUE;
        }
        if (option == "saturation") {
            sprite->penData.color.saturation += Scratch::getInputValue(block, "VALUE", sprite).asDouble();
            if (sprite->penData.color.saturation < 0) sprite->penData.color.saturation = 0;
            else if (sprite->penData.color.saturation > 100) sprite->penData.color.saturation = 100;
            return BlockResult::CONTINUE;
        }
        if (option == "brightness") {
            sprite->penData.color.brightness += Scratch::getInputValue(block, "VALUE", sprite).asDouble();
            if (sprite->penData.color.brightness < 0) sprite->penData.color.brightness = 0;
            else if (sprite->penData.color.brightness > 100) sprite->penData.color.brightness = 100;
            return BlockResult::CONTINUE;
        }
        if (option == "transparency") {
            sprite->penData.transparency += Scratch::getInputValue(block, "VALUE", sprite).asDouble();
            if (sprite->penData.transparency < 0) sprite->penData.transparency = 0;
            else if (sprite->penData.transparency > 100) sprite->penData.transparency = 100;
            return BlockResult::CONTINUE;
        }
    }
    Log::log("Unknown pen option!");

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::SetPenColorTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.color = Scratch::getInputValue(block, "COLOR", sprite).asColor();

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::SetPenSizeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.size = Scratch::getInputValue(block, "SIZE", sprite).asDouble();
    if (sprite->penData.size < minPenSize) sprite->penData.size = minPenSize;
    else if (sprite->penData.size > maxPenSize) sprite->penData.size = maxPenSize;

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::ChangePenSizeBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.size += Scratch::getInputValue(block, "SIZE", sprite).asDouble();
    if (sprite->penData.size < minPenSize) sprite->penData.size = minPenSize;
    else if (sprite->penData.size > maxPenSize) sprite->penData.size = maxPenSize;

    return BlockResult::CONTINUE;
}

#ifdef SDL_BUILD
BlockResult PenBlocks::EraseAll(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!Render::initPen()) return BlockResult::CONTINUE;
    SDL_SetRenderTarget(renderer, penTexture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, NULL);

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::Stamp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!sprite->visible || !Render::initPen()) return BlockResult::CONTINUE;

    const auto &imgFind = images.find(sprite->costumes[sprite->currentCostume].id);
    if (imgFind == images.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return BlockResult::CONTINUE;
    }

    SDL_SetRenderTarget(renderer, penTexture);

    // IDK if these are needed
    sprite->rotationCenterX = sprite->costumes[sprite->currentCostume].rotationCenterX;
    sprite->rotationCenterY = sprite->costumes[sprite->currentCostume].rotationCenterY;

    // TODO: remove duplicate code (maybe make a Render::drawSprite function.)
    SDL_Image *image = imgFind->second;
    image->freeTimer = image->maxFreeTime;
    SDL_RendererFlip flip = SDL_FLIP_NONE;

    sprite->spriteWidth = image->textureRect.w / 2;
    sprite->spriteHeight = image->textureRect.h / 2;
    if (sprite->costumes[sprite->currentCostume].isSVG) {
        sprite->spriteWidth *= 2;
        sprite->spriteHeight *= 2;
    }
    const double rotation = Math::degreesToRadians(sprite->rotation - 90.0f);
    double renderRotation = rotation;

    if (sprite->rotationStyle == sprite->LEFT_RIGHT) {
        if (std::cos(rotation) < 0) flip = SDL_FLIP_HORIZONTAL;
        renderRotation = 0;
    }
    if (sprite->rotationStyle == sprite->NONE) renderRotation = 0;

    const double rotationCenterX = (((sprite->rotationCenterX - sprite->spriteWidth)) / 2);
    const double rotationCenterY = (((sprite->rotationCenterY - sprite->spriteHeight)) / 2);

    const double offsetX = rotationCenterX * (sprite->size * 0.01);
    const double offsetY = rotationCenterY * (sprite->size * 0.01);

    const double scale = std::min(static_cast<double>(windowWidth) / Scratch::projectWidth, static_cast<double>(windowHeight) / Scratch::projectHeight);

    image->renderRect.w /= scale;
    image->renderRect.h /= scale;

    image->renderRect.x = (sprite->xPosition + 240 - (image->renderRect.w / 2)) - offsetX * std::cos(rotation) + offsetY * std::sin(renderRotation);
    image->renderRect.y = (-sprite->yPosition + 180 - (image->renderRect.h / 2)) - offsetX * std::sin(rotation) - offsetY * std::cos(renderRotation);
    const SDL_Point center = {image->renderRect.w / 2, image->renderRect.h / 2};

    // ghost effect
    SDL_SetTextureAlphaMod(image->spriteTexture, static_cast<Uint8>(255 * (1.0f - std::clamp(sprite->ghostEffect, 0.0f, 100.0f) / 100.0f)));

    SDL_RenderCopyEx(renderer, image->spriteTexture, &image->textureRect, &image->renderRect, Math::radiansToDegrees(renderRotation), &center, flip);

    image->renderRect.w *= scale;
    image->renderRect.h *= scale;

    SDL_SetRenderTarget(renderer, NULL);

    return BlockResult::CONTINUE;
}
#elif defined(__3DS__)

BlockResult PenBlocks::EraseAll(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!Render::initPen()) return BlockResult::CONTINUE;
    C3D_RenderTargetClear(penRenderTarget, C3D_CLEAR_ALL, C2D_Color32(0, 0, 0, 0), 0);
    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::Stamp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!Render::initPen()) return BlockResult::CONTINUE;
    const int SCREEN_WIDTH = 400;
    const int SCREEN_HEIGHT = 240;

    const auto &imgFind = imageC2Ds.find(sprite->costumes[sprite->currentCostume].id);
    if (imgFind == imageC2Ds.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return BlockResult::CONTINUE;
    }

    C2D_Image *costumeTexture = &imgFind->second.image;
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C3D_FrameDrawOn(penRenderTarget);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

    // TODO: remove duplicate code (maybe make a Render::drawSprite function.)

    const float scaleX = static_cast<float>(SCREEN_WIDTH) / penSubtex.width;
    const float scaleY = static_cast<float>(SCREEN_HEIGHT) / penSubtex.height;
    float spriteSize = sprite->costumes[sprite->currentCostume].isSVG ? (sprite->size * 0.01f) * 2.0f : sprite->size * 0.01f;
    const float scale = std::min(scaleX, scaleY);
    const int screenWidth = SCREEN_WIDTH;

    float rotation = Math::degreesToRadians(sprite->rotation - 90.0f);
    bool flipX = false;

    // check for rotation style
    if (sprite->rotationStyle == sprite->LEFT_RIGHT) {
        if (std::cos(rotation) < 0) {
            spriteSize *= -1;
            flipX = true;
        }
        rotation = 0;
    }
    if (sprite->rotationStyle == sprite->NONE) {
        rotation = 0;
    }

    // Center the sprite's pivot point
    double rotationCenterX = ((((sprite->rotationCenterX - sprite->spriteWidth)) / 2) * scale);
    double rotationCenterY = ((((sprite->rotationCenterY - sprite->spriteHeight)) / 2) * scale);
    if (flipX) rotationCenterX -= sprite->spriteWidth;

    const double offsetX = rotationCenterX * spriteSize;
    const double offsetY = rotationCenterY * spriteSize;

    C2D_ImageTint tinty;

    // set ghost and brightness effect
    if (sprite->brightnessEffect != 0.0f || sprite->ghostEffect != 0.0f) {
        const float brightnessEffect = sprite->brightnessEffect * 0.01f;
        const float alpha = 255.0f * (1.0f - sprite->ghostEffect / 100.0f);
        if (brightnessEffect > 0)
            C2D_PlainImageTint(&tinty, C2D_Color32(255, 255, 255, alpha), brightnessEffect);
        else
            C2D_PlainImageTint(&tinty, C2D_Color32(0, 0, 0, alpha), brightnessEffect);
    } else C2D_AlphaImageTint(&tinty, 1.0f);

    C2D_DrawImageAtRotated(
        *costumeTexture,
        static_cast<int>((sprite->xPosition * scale) + (screenWidth / 2) - offsetX * std::cos(rotation) + offsetY * std::sin(rotation)),
        static_cast<int>((sprite->yPosition * -1 * scale) + (SCREEN_HEIGHT * 0.5) - offsetX * std::sin(rotation) - offsetY * std::cos(rotation)),
        1,
        rotation,
        &tinty,
        (spriteSize)*scale / 2.0f,
        (spriteSize)*scale / 2.0f);

    return BlockResult::CONTINUE;
}
#endif
