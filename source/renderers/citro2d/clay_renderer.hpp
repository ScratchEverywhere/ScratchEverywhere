#pragma once

#include <citro2d.h>
#include <citro3d.h>
#include <clay.h>
#include <stdint.h>

// TODO: copy documentation comments from cpp file

enum {
    Clay_Citro2D_FONT_INVALID = -1,
    Clay_Citro2D_FONT_SYSTEM = 0,
};

int32_t Clay_Citro2D_RegisterFont(C2D_Font font);

Clay_Dimensions Clay_Citro2D_MeasureText(Clay_StringSlice slice, Clay_TextElementConfig *config, void *userData);

void Clay_Citro2D_Render(C3D_RenderTarget *renderTarget, Clay_Dimensions dimensions, Clay_RenderCommandArray renderCommands);
