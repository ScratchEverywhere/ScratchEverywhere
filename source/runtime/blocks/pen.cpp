#include "blockUtils.hpp"
#include <image.hpp>
#include <log.hpp>
#include <renderer.hpp>

constexpr unsigned int minPenSize = 1;
constexpr unsigned int maxPenSize = 1200;

SCRATCH_BLOCK(pen, penDown) {
    if (!Render::initPen()) return BlockResult::CONTINUE;
    sprite->penData.down = true;

    if (Scratch::accuratePen)
        Render::penDotAccurate(sprite);
    else Render::penDotFast(sprite);

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(pen, penDown);
SCRATCH_BLOCK_POSITIONAL_VOID(pen, penDown) {
    if (!Render::initPen()) return;
    sprite->penData.down = true;

    if (Scratch::accuratePen)
        Render::penDotAccurate(sprite);
    else Render::penDotFast(sprite);

    Scratch::forceRedraw = true;
}

SCRATCH_BLOCK(pen, penUp) {
    sprite->penData.down = false;

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(pen, penUp);
SCRATCH_BLOCK_POSITIONAL_VOID(pen, penUp) {
    sprite->penData.down = false;
}

SCRATCH_BLOCK(pen, setPenColorParamTo) {
    std::string option;
    double value;
    if (!Scratch::getInputValueAs(block, "COLOR_PARAM", thread, sprite, option) ||
        !Scratch::getInputValueAs(block, "VALUE", thread, sprite, value)) return BlockResult::REPEAT;

    if (option == "color") {
        double unwrappedColor = value;
        sprite->penData.color.hue = unwrappedColor - std::floor(unwrappedColor / 101) * 101;
        return BlockResult::CONTINUE;
    }
    if (option == "saturation") {
        sprite->penData.color.saturation = value;
        if (sprite->penData.color.saturation < 0) sprite->penData.color.saturation = 0;
        else if (sprite->penData.color.saturation > 100) sprite->penData.color.saturation = 100;
        return BlockResult::CONTINUE;
    }
    if (option == "brightness") {
        sprite->penData.color.brightness = value;
        if (sprite->penData.color.brightness < 0) sprite->penData.color.brightness = 0;
        else if (sprite->penData.color.brightness > 100) sprite->penData.color.brightness = 100;
        return BlockResult::CONTINUE;
    }
    if (option == "transparency") {
        sprite->penData.color.transparency = value;
        if (sprite->penData.color.transparency < 0) sprite->penData.color.transparency = 0;
        else if (sprite->penData.color.transparency > 100) sprite->penData.color.transparency = 100;
        return BlockResult::CONTINUE;
    }

    Log::log("[Pen] Unknown pen option: " + option);

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(pen, setPenColorParamTo, {"COLOR_PARAM", BytecodeStackKind::String}, {"VALUE", BytecodeStackKind::Number});
SCRATCH_BLOCK_POSITIONAL_SN_VOID(pen, setPenColorParamTo) {
    if (str == "color") {
        double unwrappedColor = num;
        sprite->penData.color.hue = unwrappedColor - std::floor(unwrappedColor / 101) * 101;
        return;
    }
    if (str == "saturation") {
        sprite->penData.color.saturation = num;
        if (sprite->penData.color.saturation < 0) sprite->penData.color.saturation = 0;
        else if (sprite->penData.color.saturation > 100) sprite->penData.color.saturation = 100;
        return;
    }
    if (str == "brightness") {
        sprite->penData.color.brightness = num;
        if (sprite->penData.color.brightness < 0) sprite->penData.color.brightness = 0;
        else if (sprite->penData.color.brightness > 100) sprite->penData.color.brightness = 100;
        return;
    }
    if (str == "transparency") {
        sprite->penData.color.transparency = num;
        if (sprite->penData.color.transparency < 0) sprite->penData.color.transparency = 0;
        else if (sprite->penData.color.transparency > 100) sprite->penData.color.transparency = 100;
        return;
    }

    Log::log("[Pen] Unknown pen option: " + str);
}

SCRATCH_BLOCK(pen, changePenColorParamBy) {
    std::string option;
    double value;

    if (!Scratch::getInputValueAs(block, "COLOR_PARAM", thread, sprite, option) ||
        !Scratch::getInputValueAs(block, "VALUE", thread, sprite, value)) return BlockResult::REPEAT;

    if (option == "color") {
        double unwrappedColor = sprite->penData.color.hue + value;
        sprite->penData.color.hue = unwrappedColor - std::floor(unwrappedColor / 101) * 101;
        return BlockResult::CONTINUE;
    }
    if (option == "saturation") {
        sprite->penData.color.saturation += value;
        if (sprite->penData.color.saturation < 0) sprite->penData.color.saturation = 0;
        else if (sprite->penData.color.saturation > 100) sprite->penData.color.saturation = 100;
        return BlockResult::CONTINUE;
    }
    if (option == "brightness") {
        sprite->penData.color.brightness += value;
        if (sprite->penData.color.brightness < 0) sprite->penData.color.brightness = 0;
        else if (sprite->penData.color.brightness > 100) sprite->penData.color.brightness = 100;
        return BlockResult::CONTINUE;
    }
    if (option == "transparency") {
        sprite->penData.color.transparency += value;
        if (sprite->penData.color.transparency < 0) sprite->penData.color.transparency = 0;
        else if (sprite->penData.color.transparency > 100) sprite->penData.color.transparency = 100;
        return BlockResult::CONTINUE;
    }
    Log::log("[Pen] Unknown pen option: " + option);

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(pen, changePenColorParamBy, {"COLOR_PARAM", BytecodeStackKind::String}, {"VALUE", BytecodeStackKind::Number});
SCRATCH_BLOCK_POSITIONAL_SN_VOID(pen, changePenColorParamBy) {
    if (str == "color") {
        double unwrappedColor = sprite->penData.color.hue + num;
        sprite->penData.color.hue = unwrappedColor - std::floor(unwrappedColor / 101) * 101;
        return;
    }
    if (str == "saturation") {
        sprite->penData.color.saturation += num;
        if (sprite->penData.color.saturation < 0) sprite->penData.color.saturation = 0;
        else if (sprite->penData.color.saturation > 100) sprite->penData.color.saturation = 100;
        return;
    }
    if (str == "brightness") {
        sprite->penData.color.brightness += num;
        if (sprite->penData.color.brightness < 0) sprite->penData.color.brightness = 0;
        else if (sprite->penData.color.brightness > 100) sprite->penData.color.brightness = 100;
        return;
    }
    if (str == "transparency") {
        sprite->penData.color.transparency += num;
        if (sprite->penData.color.transparency < 0) sprite->penData.color.transparency = 0;
        else if (sprite->penData.color.transparency > 100) sprite->penData.color.transparency = 100;
        return;
    }
    Log::log("[Pen] Unknown pen option: " + str);
}

SCRATCH_BLOCK(pen, setPenColorToColor) {
    Color color;
    if (!Scratch::getInputValueAs(block, "COLOR", thread, sprite, color)) return BlockResult::REPEAT;
    sprite->penData.color = color;
    sprite->penData.shade = sprite->penData.color.brightness / 2;
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(pen, setPenColorToColor, {"COLOR", BytecodeStackKind::Color});
SCRATCH_BLOCK_POSITIONAL_C_VOID(pen, setPenColorToColor) {
    sprite->penData.color = color;
    sprite->penData.shade = sprite->penData.color.brightness / 2;
}

SCRATCH_BLOCK(pen, setPenSizeTo) {
    double size;
    if (!Scratch::getInputValueAs(block, "SIZE", thread, sprite, size)) return BlockResult::REPEAT;

    sprite->penData.size = size;
    if (sprite->penData.size < minPenSize) sprite->penData.size = minPenSize;
    else if (sprite->penData.size > maxPenSize) sprite->penData.size = maxPenSize;

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(pen, setPenSizeTo, {"SIZE", BytecodeStackKind::Number});
SCRATCH_BLOCK_POSITIONAL_N_VOID(pen, setPenSizeTo) {
    sprite->penData.size = num;
    if (sprite->penData.size < minPenSize) sprite->penData.size = minPenSize;
    else if (sprite->penData.size > maxPenSize) sprite->penData.size = maxPenSize;
}

SCRATCH_BLOCK(pen, changePenSizeBy) {
    double size;
    if (!Scratch::getInputValueAs(block, "SIZE", thread, sprite, size)) return BlockResult::REPEAT;

    sprite->penData.size += size;
    if (sprite->penData.size < minPenSize) sprite->penData.size = minPenSize;
    else if (sprite->penData.size > maxPenSize) sprite->penData.size = maxPenSize;

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(pen, changePenSizeBy, {"SIZE", BytecodeStackKind::Number});
SCRATCH_BLOCK_POSITIONAL_N_VOID(pen, changePenSizeBy) {
    sprite->penData.size += num;
    if (sprite->penData.size < minPenSize) sprite->penData.size = minPenSize;
    else if (sprite->penData.size > maxPenSize) sprite->penData.size = maxPenSize;
}

SCRATCH_BLOCK(pen, clear) {
    if (!Render::initPen()) return BlockResult::CONTINUE;

    Render::penClear();

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(pen, clear);
SCRATCH_BLOCK_POSITIONAL_VOID(pen, clear) {
    if (!Render::initPen()) return;
    Render::penClear();
    Scratch::forceRedraw = true;
}

SCRATCH_BLOCK(pen, stamp) {
    if (!Render::initPen()) return BlockResult::CONTINUE;

    Scratch::loadCurrentCostumeImage(sprite);

    Render::penStamp(sprite);

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(pen, stamp);
SCRATCH_BLOCK_POSITIONAL_VOID(pen, stamp) {
    if (!Render::initPen()) return;
    Scratch::loadCurrentCostumeImage(sprite);
    Render::penStamp(sprite);
    Scratch::forceRedraw = true;
}

SCRATCH_BLOCK(pen, setPenHueToNumber) {
    double hue;
    if (!Scratch::getInputValueAs(block, "HUE", thread, sprite, hue)) return BlockResult::REPEAT;

    double unwrappedColor = hue / 2;
    sprite->penData.color.hue = unwrappedColor - std::floor(unwrappedColor / 101) * 101;
    sprite->penData.color.transparency = 0;
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(pen, setPenHueToNumber, {"HUE", BytecodeStackKind::Number});
SCRATCH_BLOCK_POSITIONAL_N_VOID(pen, setPenHueToNumber) {
    double unwrappedColor = num / 2;
    sprite->penData.color.hue = unwrappedColor - std::floor(unwrappedColor / 101) * 101;
    sprite->penData.color.transparency = 0;
}

SCRATCH_BLOCK(pen, changePenHueBy) {
    double hue;
    if (!Scratch::getInputValueAs(block, "HUE", thread, sprite, hue)) return BlockResult::REPEAT;

    double unwrappedColor = sprite->penData.color.hue + hue / 2;
    sprite->penData.color.hue = unwrappedColor - std::floor(unwrappedColor / 101) * 101;

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(pen, changePenHueBy, {"HUE", BytecodeStackKind::Number});
SCRATCH_BLOCK_POSITIONAL_N_VOID(pen, changePenHueBy) {
    double unwrappedColor = sprite->penData.color.hue + num / 2;
    sprite->penData.color.hue = unwrappedColor - std::floor(unwrappedColor / 101) * 101;
}

SCRATCH_BLOCK(pen, setPenShadeToNumber) {
    double shade;
    if (!Scratch::getInputValueAs(block, "SHADE", thread, sprite, shade)) return BlockResult::REPEAT;

    sprite->penData.shade = std::fmod(shade, 200);
    if (sprite->penData.shade < 0) sprite->penData.shade += 200;

    sprite->penData.color = legacyUpdatePenColor(sprite->penData.color, sprite->penData.shade);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(pen, setPenShadeToNumber, {"SHADE", BytecodeStackKind::Number});
SCRATCH_BLOCK_POSITIONAL_N_VOID(pen, setPenShadeToNumber) {
    sprite->penData.shade = std::fmod(num, 200);
    if (sprite->penData.shade < 0) sprite->penData.shade += 200;

    sprite->penData.color = legacyUpdatePenColor(sprite->penData.color, sprite->penData.shade);
}

SCRATCH_BLOCK(pen, changePenShadeBy) {
    double shade;
    if (!Scratch::getInputValueAs(block, "SHADE", thread, sprite, shade)) return BlockResult::REPEAT;

    sprite->penData.shade += shade;
    sprite->penData.shade = std::fmod(sprite->penData.shade, 200);
    if (sprite->penData.shade < 0) sprite->penData.shade += 200;

    sprite->penData.color = legacyUpdatePenColor(sprite->penData.color, sprite->penData.shade);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(pen, changePenShadeBy, {"SHADE", BytecodeStackKind::Number});
SCRATCH_BLOCK_POSITIONAL_N_VOID(pen, changePenShadeBy) {
    sprite->penData.shade += num;
    sprite->penData.shade = std::fmod(sprite->penData.shade, 200);
    if (sprite->penData.shade < 0) sprite->penData.shade += 200;

    sprite->penData.color = legacyUpdatePenColor(sprite->penData.color, sprite->penData.shade);
}

SCRATCH_SHADOW_BLOCK(pen_menu_colorParam, colorParam)
