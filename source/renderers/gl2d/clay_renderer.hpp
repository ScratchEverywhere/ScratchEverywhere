#pragma once

#include <clay.h>
#include <gl2d.h>
#include <nds.h>
#include <stdint.h>

// NOTE: this function will automatically put RomFS location and '.ttf' in fontPath, so don't provide those yourself in the argument.
bool Clay_GL2D_RegisterFont(const std::string &fontPath);

Clay_Dimensions Clay_GL2D_MeasureText(Clay_StringSlice slice, Clay_TextElementConfig *config, void *userData);

void Clay_GL2D_Render(Clay_Dimensions dimensions, Clay_RenderCommandArray renderCommands);
