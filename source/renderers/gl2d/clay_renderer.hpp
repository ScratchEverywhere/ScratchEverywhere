#pragma once

#include <clay.h>
#include <gl2d.h>
#include <nds.h>
#include <stdint.h>

// NOTE: this function will automatically put RomFS location and '.ttf' in fontPath, so don't provide those yourself in the argument.
bool Clay_GL2D_RegisterFont(const std::string &fontPath);

// Frees the registered fonts' textures. Must be called before the GL context
// / video hardware is torn down, for the same reason as the other GL-family
// renderers: the fonts are held in a static vector, so its destructor at
// process exit would otherwise run too late.
void Clay_GL2D_FreeFonts();

Clay_Dimensions Clay_GL2D_MeasureText(Clay_StringSlice slice, Clay_TextElementConfig *config, void *userData);

void Clay_GL2D_Render(Clay_Dimensions dimensions, Clay_RenderCommandArray renderCommands);
