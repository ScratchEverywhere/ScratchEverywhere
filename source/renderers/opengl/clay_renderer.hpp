#pragma once

#include <clay.h>
#include <string>

bool Clay_OpenGL_RegisterFont(const std::string &fontPath);

// Frees the registered fonts' textures. Must be called while the GL context
// is still current (i.e. before window/context teardown) - the fonts are
// held in a static vector, so relying on its destructor at process exit runs
// too late and crashes trying to call GL functions on a dead context.
void Clay_OpenGL_FreeFonts();

Clay_Dimensions Clay_OpenGL_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData);

void Clay_OpenGL_Render(Clay_Dimensions dimensions, Clay_RenderCommandArray commands);
