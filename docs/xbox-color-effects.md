# Xbox color, brightness and pen compositing

Tetris uses one red costume with seven color effects for its pieces. The Xbox
renderer previously omitted `Sprite::colorEffect` for both ordinary sprites
and pen stamps, leaving every piece red. Brightness also used a multipass
approximation rather than Scratch's additive RGB adjustment.

The Xbox image path now applies color and brightness to source RGBA pixels and
caches the resulting GPU tiles. A shared transform also updates CPU color
sensing. Color is applied before brightness, with Scratch's low-value and
low-saturation floors; nonzero full hue turns retain those floors. Source alpha
is preserved and ghost remains a separate draw-time opacity. See the upstream
[sprite shader](https://github.com/scratchfoundation/scratch-render/blob/develop/src/shaders/sprite.frag)
and [effect conversion](https://github.com/scratchfoundation/scratch-render/blob/develop/src/ShaderManager.js).

Effect tiles use at most 256×256 pixels. Their LRU cache is limited to 8MiB of
page/POT-rounded pixel storage and 128 entries. Refreshing or destroying an image
removes its entries. Costumes without effects retain the existing path. Failed
effect allocations log once per image rather than silently growing the cache.

## Alpha and readback defects found by the fixture

A half-transparent colored stamp initially failed while the other 106 pixel
samples passed. NV2A uses the same blend factors for RGB and alpha. The SDL port
used `SRC_ALPHA` for both, which squared alpha in transparent targets. Displaying
the pen target with ordinary straight-alpha blending then multiplied its stored
RGB by alpha again.

The SDL dependency patch now premultiplies RGB in the final combiner for
`SDL_BLENDMODE_BLEND`, preserving alpha, then blends with `ONE` and
`ONE_MINUS_SRC_ALPHA`. The pen target is composited with
`SDL_BLENDMODE_BLEND_PREMULTIPLIED`. Already-premultiplied input is not multiplied
again. The first blend selection is initialized explicitly rather than assuming
the zero-initialized state matches GPU state.

The expanded GPU probe also exposed two `SDL_RenderReadPixels` defects: it read
the main backbuffer even when a texture was targeted, and it applied a requested
subrectangle's offset to the rect-sized destination allocation. Readback now
uses the active target's pixels/format/pitch, or the backbuffer's actual pitch,
and writes at destination origin. Conversion errors release the surface.

## Reproduction and acceptance

Host math check:

```sh
python3 tests/color_effects_test.py
```

It compares 1,756 C++ results with an independent Python HSV reference. It
includes the Tetris hues, negative/full hue turns, grayscale floors, brightness
limits and combinations. The observed maximum channel difference was 1/255.

Generate the integration fixture:

```sh
python3 examples/make-color-effects-test.py /tmp/Color-Effects-Test.sb3
```

Expected monitors are **PASSES=5, ERRORS=0, CYCLES=40**. The companion JSON specifies
107 independent pixel samples covering color/brightness/ghost, partial source
alpha, shared texture-state changes, pen stamps and seams of a tiled image.
The cycling 512×512 costume exceeds the cache budget across its hue changes.
The fixture finishes its scripts without `stop all`, which would clear effects.

Enable `SE_XBOX_TRACE_RENDERER` for fourteen GPU checks: the original eight
startup/viewport/target checks, target alpha and overlapping alpha, their
premultiplied composites, and nonzero-origin target/main readback. It also logs
effect-cache counters every 120 presents. This option remains OFF by default.

For an unmodified 720×480 KVM image:

```sh
python3 tests/color_effects_pixels.py capture.jpg /tmp/Color-Effects-Test.expected.json
```

A native 640×480 framebuffer is also supported. Supply its actual viewport when
overscan is active, for example `--viewport 25 19 590 442` for the tested xemu
4% safe area. This maps sample coordinates; it does not change captured pixels.

The fixture and symbol-bearing builds are diagnostic artifacts, not one of the
twenty qualified games. Tetris gameplay, physical-console colors/pen behavior,
normal browser exit/relaunch and audio remain hardware acceptance gates.

## Observed emulator results

The final diagnostic XBE is 9,367,552 bytes, SHA-256
`bfade7d7c9eb86934be5a74798df49b54c8ddcd0a8122f4848d2442f235e1ee2`.
It ran in an isolated xemu guest with its matching DWARF companion. Networking
was skipped under GDB; the existing user's emulator/disk/config were preserved.

- All fourteen GPU probes passed, including alpha 128 on a transparent target,
  alpha 192 after overlap, and alpha 255 after composition over opaque blue.
- The 107 image samples passed with maximum channel error 1. Visible monitors
  reported PASSES=5, ERRORS=0, CYCLES=40.
- The cache peaked at 8,273,920 pixel bytes, created 181 tiles and evicted 138.
  It settled at 43 entries/8,269,824 bytes. Invoking the real
  `cleanupScratchProject()` through GDB reduced both bytes and entries to zero.
  This tests destruction; it does not replace a controller-driven browser exit.
- Applying the dependency patch to clean files from its pinned revision produced
  byte-identical copies of all five compiled dependency files.

Ignored evidence is in `build-xbox/xemu-debug/effects-final/`, the build manifest
`effects-diagnostic-build.json`, and `effects-final-patch-verification.json`.
The preceding `effects-before-alpha/` capture retains the failing half-opacity
stamp (106/107), along with its executable, ISO and symbols. The physical Xbox
still has its prior verified production build; these changes have not been
deployed there.
