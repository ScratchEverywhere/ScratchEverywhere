#pragma once
#include <SDL2/SDL.h>
#ifdef ENABLE_AUDIO
#include <SDL_mixer.h>
#endif
#include <SDL_ttf.h>

#define CONTROLLER_DEADZONE_X 10000
#define CONTROLLER_DEADZONE_Y 10000
#define CONTROLLER_DEADZONE_TRIGGER 1000

extern int windowWidth;
extern int windowHeight;
extern SDL_Window *window;
extern SDL_Renderer *renderer;

std::pair<float, float> screenToScratchCoords(float screenX, float screenY, int windowWidth, int windowHeight);
