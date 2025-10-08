#pragma once
#include "../blockExecutor.hpp"

#ifdef __3DS__
#include <citro2d.h>
#include <citro3d.h>

extern C2D_Image penImage;
extern C3D_RenderTarget *penRenderTarget;
extern Tex3DS_SubTexture penSubtex;
extern C3D_Tex *penTex;
#elif defined(SDL_BUILD)
#include <SDL2/SDL.h>

extern SDL_Texture *penTexture;
#else
#error Unsupported Platform.
#endif

class PenBlocks {
  public:
    static BlockResult PenDown(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult PenUp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult EraseAll(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult SetPenOptionTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult ChangePenOptionBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult Stamp(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult SetPenColorTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult SetPenSizeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
    static BlockResult ChangePenSizeBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat);
};
