#pragma once

#include <clay.h>
#include <string>

bool Clay_OpenGLCore_RegisterFont(const std::string &fontPath);

// Frees the registered fonts' textures and the shared shader/VAO/VBO. Must be
// called while the GL context is still current (i.e. before window/context
// teardown) - these are held in static state, so relying on their destructors
// at process exit runs too late and crashes trying to call GL functions on a
// dead context.
void Clay_OpenGLCore_FreeFonts();

Clay_Dimensions Clay_OpenGLCore_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData);

void Clay_OpenGLCore_Render(Clay_Dimensions dimensions, Clay_RenderCommandArray commands);
