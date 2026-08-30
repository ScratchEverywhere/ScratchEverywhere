#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <clay.h>

typedef struct
{
    uint32_t fontId;
    TTF_Font *font;
} SDL_Font;

Clay_Dimensions SDL_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData);

void Clay_SDL_Render(SDL_Surface *surface, Clay_RenderCommandArray renderCommands, SDL_Font *fonts);
