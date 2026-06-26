#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <clay.h>

typedef struct
{
    uint32_t fontId;
    TTF_Font *font;
} SDL_Font;

Clay_Dimensions SDL_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData);

void Clay_SDL_Render(SDL_Renderer *renderer, Clay_RenderCommandArray renderCommands, SDL_Font *fonts);
