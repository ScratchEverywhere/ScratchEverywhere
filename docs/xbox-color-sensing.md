# Xbox color-sensing compatibility

The Maze starter reproduced a collision defect: the player crossed the entire
blue wall while left-stick input worked. The normal Xbox loader frees backdrop
RGBA after upload. Compressed PNG caches also keep GPU data and optional alpha
masks, without CPU colors. Color sensing then either cannot construct a mask
or reads black in place of the original colors.

`Image::ensureColorData` lets color queries request CPU colors. The Xbox
implementation decodes the original asset only when needed, matches the current
texture dimensions and scale, and preserves GPU resources. This avoids using
lossy DXT or dithered texture colors for Scratch color comparisons. Restored
buffers remain owned by the image and are freed on destruction/cache refresh.
Failed restoration is logged once per image rather than retried for every
pixel. Ordinary alpha-only collision queries retain their memory savings.

Color queries also build a conservative candidate list once. Candidates whose
existing collision circles cannot overlap the source circle are skipped; the
remaining sprites keep their original compositing order. Previously every
sampled pixel scanned all sprites, including dozens of distant PacMan dots.

## Evidence

The first hardware repair restored the Maze SVG backdrop colors and PacMan
cached PNG colors. The retrieved `pacman-before-cull.log` records both decodes.
Maze stopped at the first wall and completed its intended route to You win.
PacMan steered, rejected walls and reached 70 points. The first repair alone
still showed slow PacMan movement; culling needs a separate gameplay check.

The regression generator is `examples/make-color-sensing-test.py` (Python
standard library only). It covers cached PNG and freed SVG backdrops, masked
colors, a rotated overlay, a transparent overlay, a disjoint overlay and 90
distant visible sprites. It is a test fixture, not a qualification game.
Expected monitors: PASSES=14, ERRORS=0.

An initial hardware run returned PASSES=13, ERRORS=1. The failing case was
layer-not-white: a rotated green overlay still exposed a white sample. The
integer coordinate trace found the exact sample at (90,-8): the overlay was
centered at (100,0), direction 135, with collision scale 15 and a 2×2 mask.
Its shared SVG had first been rasterized by a small distant sprite to 5×5.
Integer division by two discarded its final row/column, rejecting this covered
point. Both collision-mask allocation paths now round grid dimensions upward,
so the final partial cells remain sampleable. No source pixels outside the
image are accessed. Temporary coordinate logging was removed.


The final production build passed **14/14 cases twice**, including a short-Start
replay that switches back from SVG to PNG and resets the overlay. Both captures
show PASSES=14, ERRORS=0 and FAIL CASE=0:
`color-regression-final-pass.jpg` and `color-regression-final-replay.jpg` in
ignored `build-xbox/hardware`. The test is deliberately excluded from the
20-game catalog.

The executable was fully read back after installation at
`F:\Games\ScratchEverywhere\default.xbe`, SHA-256
`11836d5c94fb63642c5591dfcf0626845ba80a4aba83712dae9b8d037beb1a3c`
(9,469,952 bytes). Native allocation, expanded RAM and 480p are enabled;
renderer/gameplay diagnostics and temporary coordinate tracing are disabled.


PacMan's repeated 1.2-second right-input check moved its yellow center from
approximately X=362 to X=416 after the full repair, versus X=390 before culling
(54 versus 28 pixels of travel). This is a bounded gameplay comparison, not a
measured FPS benchmark; ghost paths and collision workload vary. Evidence:
`pacman-cull-start.jpg`, `pacman-cull-right-1200.jpg`,
`pacman-moving-score.jpg`, and `pacman-cull-comparison.json`.
