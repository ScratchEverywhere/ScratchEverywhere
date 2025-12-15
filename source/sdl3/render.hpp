#pragma once
#include <SDL3/SDL.h>
#include <utility>
#ifdef ENABLE_AUDIO
#include <SDL3_mixer/SDL_mixer.h>
#endif
#include <SDL3_ttf/SDL_ttf.h>

extern int windowWidth;
extern int windowHeight;
extern SDL_Window *window;
extern SDL_Renderer *renderer;
