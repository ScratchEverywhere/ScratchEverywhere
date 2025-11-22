#include "pen.hpp"
#include "../image.hpp"
#include "../interpret.hpp"
#include "../math.hpp"
#include "../render.hpp"
#include "unzip.hpp"

#ifdef __3DS__
#include "../../3ds/image.hpp"
#include <citro2d.h>
#include <citro3d.h>
C2D_Image penImage;
C3D_RenderTarget *penRenderTarget;
Tex3DS_SubTexture penSubtex;
C3D_Tex *penTex;
#elif defined(RENDERER_SDL2)
#include "../../sdl2/image.hpp"
#include "../../sdl2/render.hpp"
#include <SDL2_gfxPrimitives.h>

SDL_Texture *penTexture;
#else
#warning Unsupported Platform for pen.
#endif

const unsigned int minPenSize = 1;
const unsigned int maxPenSize = 1200;

BlockResult PenBlocks::PenDown(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!Render::initPen()) return BlockResult::CONTINUE;
    sprite->penData.down = true;

#ifdef RENDERER_SDL2
    int penWidth;
    int penHeight;
    SDL_QueryTexture(penTexture, NULL, NULL, &penWidth, &penHeight);

    SDL_Texture *tempTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, penWidth, penHeight);
    SDL_SetTextureBlendMode(tempTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(tempTexture, (100 - sprite->penData.color.transparency) / 100.0f * 255);
    SDL_SetRenderTarget(renderer, tempTexture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    const double scale = (penHeight / static_cast<double>(Scratch::projectHeight));

    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    filledCircleRGBA(renderer, sprite->xPosition * scale + penWidth / 2.0f, -sprite->yPosition * scale + penHeight / 2.0f, (sprite->penData.size / 2.0f) * scale, rgbColor.r, rgbColor.g, rgbColor.b, 255);

    SDL_SetRenderTarget(renderer, penTexture);
    SDL_RenderCopy(renderer, tempTexture, NULL, NULL);
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_DestroyTexture(tempTexture);
#elif defined(__3DS__)
    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    if (!Render::hasFrameBegan) {
        if (!C3D_FrameBegin(C3D_FRAME_NONBLOCK)) C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        Render::hasFrameBegan = true;
    }
    C2D_SceneBegin(penRenderTarget);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

    const int SCREEN_WIDTH = Render::getWidth();
    const int SCREEN_HEIGHT = Render::getHeight();

    const u32 color = C2D_Color32(rgbColor.r, rgbColor.g, rgbColor.b, rgbColor.a);
    const int thickness = std::clamp(static_cast<int>(sprite->penData.size * Render::renderScale), 1, 1000);

    const float xSscaled = (sprite->xPosition * Render::renderScale) + (SCREEN_WIDTH / 2);
    const float yScaled = (sprite->yPosition * -1 * Render::renderScale) + (SCREEN_HEIGHT * 0.5);
    const float radius = thickness / 2.0f;

    C2D_DrawCircleSolid(xSscaled, yScaled + TEXTURE_OFFSET, 0, radius, color);
#endif

    Scratch::forceRedraw = true;
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
            double unwrappedColor = Scratch::getInputValue(block, "VALUE", sprite).asDouble();
            sprite->penData.color.hue = unwrappedColor - std::floor(unwrappedColor / 101) * 101;
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
            sprite->penData.color.transparency = Scratch::getInputValue(block, "VALUE", sprite).asDouble();
            if (sprite->penData.color.transparency < 0) sprite->penData.color.transparency = 0;
            else if (sprite->penData.color.transparency > 100) sprite->penData.color.transparency = 100;
            return BlockResult::CONTINUE;
        }
    }

    Log::log("Unknown pen option!");

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::ChangePenOptionBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {

    Block *optionBlock = findBlock(Scratch::getInputValue(block, "COLOR_PARAM", sprite).asString());

    if (optionBlock != nullptr) {

        const std::string option = Scratch::getFieldValue(*optionBlock, "colorParam");

        if (option == "color") {
            double unwrappedColor = sprite->penData.color.hue + Scratch::getInputValue(block, "VALUE", sprite).asDouble();
            sprite->penData.color.hue = unwrappedColor - std::floor(unwrappedColor / 101) * 101;
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
            sprite->penData.color.transparency += Scratch::getInputValue(block, "VALUE", sprite).asDouble();
            if (sprite->penData.color.transparency < 0) sprite->penData.color.transparency = 0;
            else if (sprite->penData.color.transparency > 100) sprite->penData.color.transparency = 100;
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

#ifdef RENDERER_SDL2
BlockResult PenBlocks::EraseAll(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!Render::initPen()) return BlockResult::CONTINUE;
    SDL_SetRenderTarget(renderer, penTexture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, NULL);

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::Stamp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!Render::initPen()) return BlockResult::CONTINUE;

    Image::loadImageFromProject(sprite);

    const auto &imgFind = images.find(sprite->costumes[sprite->currentCostume].id);
    if (imgFind == images.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return BlockResult::CONTINUE;
    }
    imgFind->second->freeTimer = imgFind->second->maxFreeTime;

    SDL_SetRenderTarget(renderer, penTexture);

    // IDK if these are needed
    sprite->rotationCenterX = sprite->costumes[sprite->currentCostume].rotationCenterX;
    sprite->rotationCenterY = sprite->costumes[sprite->currentCostume].rotationCenterY;

    // TODO: remove duplicate code (maybe make a Render::drawSprite function.)
    SDL_Image *image = imgFind->second;
    image->freeTimer = image->maxFreeTime;
    sprite->rotationCenterX = sprite->costumes[sprite->currentCostume].rotationCenterX;
    sprite->rotationCenterY = sprite->costumes[sprite->currentCostume].rotationCenterY;
    sprite->spriteWidth = image->textureRect.w >> 1;
    sprite->spriteHeight = image->textureRect.h >> 1;
    SDL_RendererFlip flip = SDL_FLIP_NONE;
    const bool isSVG = sprite->costumes[sprite->currentCostume].isSVG;
    Render::calculateRenderPosition(sprite, isSVG);
    image->renderRect.x = sprite->renderInfo.renderX;
    image->renderRect.y = sprite->renderInfo.renderY;

    if (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0) {
        flip = SDL_FLIP_HORIZONTAL;
        image->renderRect.x += (sprite->spriteWidth * (isSVG ? 2 : 1)) * 1.125; // Don't ask why I'm multiplying by 1.125 here, I also have no idea, but it makes it work so...
    }

    // Pen mapping stuff
    const auto &cords = screenToScratchCoords(image->renderRect.x, image->renderRect.y, windowWidth, windowHeight);
    image->renderRect.x = cords.first + Scratch::projectWidth / 2;
    image->renderRect.y = -cords.second + Scratch::projectHeight / 2;

    if (Scratch::hqpen) {
        image->setScale(sprite->renderInfo.renderScaleY);

        int penWidth;
        int penHeight;
        SDL_QueryTexture(penTexture, NULL, NULL, &penWidth, &penHeight);
        const double scale = (penHeight / static_cast<double>(Scratch::projectHeight));

        image->renderRect.x *= scale;
        image->renderRect.y *= scale;
    } else {
        image->setScale(sprite->size / (isSVG ? 100.0f : 200.0f));
    }

    // set ghost effect
    float ghost = std::clamp(sprite->ghostEffect, 0.0f, 100.0f);
    Uint8 alpha = static_cast<Uint8>(255 * (1.0f - ghost / 100.0f));
    SDL_SetTextureAlphaMod(image->spriteTexture, alpha);

    // set brightness effect
    if (sprite->brightnessEffect != 0) {
        float brightness = sprite->brightnessEffect * 0.01f;
        if (brightness > 0.0f) {
            // render the normal image first
            SDL_RenderCopyEx(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                             Math::radiansToDegrees(sprite->renderInfo.renderRotation), nullptr, flip);

            // render another, blended image on top
            SDL_SetTextureBlendMode(image->spriteTexture, SDL_BLENDMODE_ADD);
            SDL_SetTextureAlphaMod(image->spriteTexture, (Uint8)(brightness * 255 * (alpha / 255.0f)));
            SDL_RenderCopyEx(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                             Math::radiansToDegrees(sprite->renderInfo.renderRotation), nullptr, flip);

            // reset for next frame
            SDL_SetTextureBlendMode(image->spriteTexture, SDL_BLENDMODE_BLEND);
        } else {
            Uint8 col = static_cast<Uint8>(255 * (1.0f + brightness));
            SDL_SetTextureColorMod(image->spriteTexture, col, col, col);

            SDL_RenderCopyEx(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                             Math::radiansToDegrees(sprite->renderInfo.renderRotation), nullptr, flip);
            // reset for next frame
            SDL_SetTextureColorMod(image->spriteTexture, 255, 255, 255);
        }
    } else {
        // if no brightness just render normal image
        SDL_SetTextureColorMod(image->spriteTexture, 255, 255, 255);
        SDL_RenderCopyEx(renderer, image->spriteTexture, &image->textureRect, &image->renderRect,
                         Math::radiansToDegrees(sprite->renderInfo.renderRotation), nullptr, flip);
    }

    SDL_SetRenderTarget(renderer, NULL);

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}
#elif defined(__3DS__)

BlockResult PenBlocks::EraseAll(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!Render::initPen()) return BlockResult::CONTINUE;
    C2D_TargetClear(penRenderTarget, C2D_Color32(0, 0, 0, 0));

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::Stamp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!Render::initPen()) return BlockResult::CONTINUE;

    Image::loadImageFromProject(sprite);

    const auto &imgFind = images.find(sprite->costumes[sprite->currentCostume].id);
    if (imgFind == images.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return BlockResult::CONTINUE;
    }
    ImageData &data = imgFind->second;
    imgFind->second.freeTimer = data.maxFreeTimer;
    C2D_Image *costumeTexture = &data.image;
    if (!Render::hasFrameBegan) {
        if (!C3D_FrameBegin(C3D_FRAME_NONBLOCK)) C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        Render::hasFrameBegan = true;
    }
    C2D_SceneBegin(penRenderTarget);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);

    const bool isSVG = data.isSVG;
    sprite->rotationCenterX = sprite->costumes[sprite->currentCostume].rotationCenterX;
    sprite->rotationCenterY = sprite->costumes[sprite->currentCostume].rotationCenterY;
    sprite->spriteWidth = data.width >> 1;
    sprite->spriteHeight = data.height >> 1;
    Render::calculateRenderPosition(sprite, isSVG);

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
        sprite->renderInfo.renderX,
        sprite->renderInfo.renderY + TEXTURE_OFFSET,
        1,
        sprite->renderInfo.renderRotation,
        &tinty,
        sprite->renderInfo.renderScaleX,
        sprite->renderInfo.renderScaleY);

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

#else
BlockResult PenBlocks::EraseAll(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::Stamp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

#endif
