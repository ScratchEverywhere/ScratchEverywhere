#pragma once

#include <clay.h>
#include <string>

bool Clay_OpenGLCore_RegisterFont(const std::string &fontPath);

Clay_Dimensions Clay_OpenGLCore_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData);

void Clay_OpenGLCore_Render(Clay_Dimensions dimensions, Clay_RenderCommandArray commands);
