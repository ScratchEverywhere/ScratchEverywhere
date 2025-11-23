#pragma once
#include "blockUtils.hpp"

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

namespace blocks::pen {
BlockResult PenDown(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult PenUp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult EraseAll(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult SetPenOptionTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult ChangePenOptionBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult Stamp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult SetPenColorTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult SetPenSizeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
BlockResult ChangePenSizeBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
} // namespace blocks::pen
