#pragma once
#include "image.hpp"
#include <SDL2/SDL.h>
#ifdef ENABLE_AUDIO
#include <SDL2/SDL_mixer.h>
#endif
#include <SDL2/SDL_ttf.h>

extern int windowWidth;
extern int windowHeight;
extern SDL_Window *window;
extern SDL_Renderer *renderer;