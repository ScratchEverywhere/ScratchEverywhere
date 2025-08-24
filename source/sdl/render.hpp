#pragma once
#include "image.hpp"
#ifdef ENABLE_AUDIO
#ifdef XBOX
#include <SDL_mixer.h>
#else
#include <SDL2/SDL_mixer.h>
#endif
#endif

#ifdef XBOX
#include <SDL.h>
#include <SDL_ttf.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#endif

extern int windowWidth;
extern int windowHeight;
extern SDL_Window *window;
extern SDL_Renderer *renderer;

std::pair<float, float> screenToScratchCoords(float screenX, float screenY, int windowWidth, int windowHeight);
