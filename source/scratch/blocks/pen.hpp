#pragma once

// TODO: Move this out of here, or just um, merge the SDL3 PR...
#ifdef __3DS__
#include <citro2d.h>
#include <citro3d.h>

extern C2D_Image penImage;
extern C3D_RenderTarget *penRenderTarget;
extern Tex3DS_SubTexture penSubtex;
extern C3D_Tex *penTex;

#define TEXTURE_OFFSET 16

#elif defined(SDL_BUILD)
#include <SDL2/SDL.h>

extern SDL_Texture *penTexture;
#else
#warning Unsupported platform for pen.
#endif
