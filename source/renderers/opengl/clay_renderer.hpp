#pragma once

#include <clay.h>
#include <string>

bool Clay_OpenGL_RegisterFont(const std::string &fontPath);

Clay_Dimensions Clay_OpenGL_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData);

void Clay_OpenGL_Render(Clay_Dimensions dimensions, Clay_RenderCommandArray commands);
