#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <clay.h>

typedef struct
{
    uint32_t fontId;
    TTF_Font *font;
} SDL2_Font;

#ifdef __cplusplus
extern "C" {
#endif

Clay_Dimensions SDL2_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData);

void Clay_SDL2_Render(SDL_Renderer *renderer, Clay_RenderCommandArray renderCommands, SDL2_Font *fonts);

#ifdef __cplusplus
}
#endif
