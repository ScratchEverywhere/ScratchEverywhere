#include "pen.hpp"
#include "../image.hpp"
#include "../interpret.hpp"
#include "../math.hpp"
#include "../render.hpp"
#include "unzip.hpp"

const unsigned int minPenSize = 1;
const unsigned int maxPenSize = 1200;

BlockResult PenBlocks::PenDown(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!Render::initPen()) return BlockResult::CONTINUE;
    sprite->penData.down = true;

    Render::penDot(sprite);

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::PenUp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.down = false;

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::SetPenOptionTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    const std::string option = Scratch::getInputValue(block, "COLOR_PARAM", sprite).asString();

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

    Log::log("Unknown pen option!");

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::ChangePenOptionBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {

    const std::string option = Scratch::getInputValue(block, "COLOR_PARAM", sprite).asString();

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
    Log::log("Unknown pen option!");

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::SetPenColorTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.color = Scratch::getInputValue(block, "COLOR", sprite).asColor();
    sprite->penData.shade = sprite->penData.color.brightness / 2;

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

BlockResult PenBlocks::EraseAll(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!Render::initPen()) return BlockResult::CONTINUE;

    Render::penClear();

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::Stamp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!Render::initPen()) return BlockResult::CONTINUE;

    Image::loadImageFromProject(sprite);

    Render::penStamp(sprite);

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::SetPenHueToNumber(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    double unwrappedColor = Scratch::getInputValue(block, "HUE", sprite).asDouble() / 2;
    sprite->penData.color.hue = unwrappedColor - std::floor(unwrappedColor / 101) * 101;
    sprite->penData.color.transparency = 0;

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::ChangePenHueBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    double unwrappedColor = sprite->penData.color.hue + Scratch::getInputValue(block, "HUE", sprite).asDouble() / 2;
    sprite->penData.color.hue = unwrappedColor - std::floor(unwrappedColor / 101) * 101;

    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::SetPenShadeToNumber(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->penData.shade = std::fmod(Scratch::getInputValue(block, "SHADE", sprite).asDouble(), 200);
    if (sprite->penData.shade < 0) sprite->penData.shade += 200;

    sprite->penData.color = legacyUpdatePenColor(sprite->penData.color, sprite->penData.shade);
    return BlockResult::CONTINUE;
}

BlockResult PenBlocks::ChangePenShadeBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {

    sprite->penData.shade += Scratch::getInputValue(block, "SHADE", sprite).asDouble();
    sprite->penData.shade = std::fmod(sprite->penData.shade, 200);
    if (sprite->penData.shade < 0) sprite->penData.shade += 200;

    sprite->penData.color = legacyUpdatePenColor(sprite->penData.color, sprite->penData.shade);
    return BlockResult::CONTINUE;
}
