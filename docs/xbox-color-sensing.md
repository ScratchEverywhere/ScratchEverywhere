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

## Pen-layer collision repair (October 27)

Breakout's visually erased blue row still bounced the ball and awarded points.
In an isolated normal-build run, controller inputs kept all three lives through
Score 1100, while successive hits stayed at the erased row. Color sensing
composited sprite/backdrop pixels but omitted the pen canvas, which this game
uses for black eraser stamps.

The Xbox renderer now prepares a regional readback of the pen render target
for each color-query area. Pending line geometry is flushed first. Repeated
queries reuse the region until a line, dot, stamp or clear changes the canvas;
cleanup and resize discard the cache. Both color reporters composite the
premultiplied pen pixels above the backdrop and below sprites. Other renderers
retain their existing behavior through optional no-op renderer methods.

`examples/make-color-sensing-test.py` now generates 34 checks, retaining the
previous fourteen and adding opaque/transparent stamps, persistence after the
stamper moves, dots, lines, clear invalidation, masked queries and sprite
layering. The identical fixture returned PASSES=23 / ERRORS=11 on the preceding
normal executable and PASSES=34 / ERRORS=0 on the candidate. For the translucent
case, texture opacity truncates 50% to 127/255: the measured stored green and
alpha bytes are both 127, yielding RGB (11,130,128) over the blue backdrop.
The expected fixture color uses that measured conversion.

The separate NXDK build passed. Candidate XBE SHA-256:
`2141b1a044f9d08c40286d02f52de72dec8c9e0894c9938223e725cb529763e4`.
Matching ELF SHA-256:
`5b06fca7511bb1a0d3a6eb1c82a7020993ac19f9add36417f26f56d6be638b1b`.
Evidence is retained under ignored `build-xbox/pen-color-20261027/`, including
identical-fixture manifests, counters and framebuffer captures. The GDB tests
bypass network initialization only; fixture game state is not overridden.
The candidate has not yet been installed on the physical Xbox.

Full-board Breakout completion remains open. A subsequent ordinary-controller
run with a shallow random launch angle exposed a separate project-script issue:
the ball hits the blue band near Y=53.31, while the eraser rounds its center to
Y=44 and stamps below the band (which spans Y=56–78). All three lives remained
at Score 320, but the blue row stayed intact. A full pen-canvas readback found
no alpha pixels in that band. The eraser's grid selection needs a separate
repair; the pen-layer engine fix alone does not qualify the full game.

### Exact bitmap sample positions

After the pen repair, a normal controller run visually erased every Breakout
row with all three lives, but the score kept increasing. GDB stopped at a true
red-color result for world point (-241,160), one unit outside the 480-unit
backdrop. The reduced collision grid snapped that coordinate to the first
source column. A direct call confirmed this NXDK's `roundf(-0.5)` returns zero
(the SDK implements rounding to nearest/even). The pen sampler correctly
rejected the out-of-canvas point, so the uncovered backdrop sample became a
phantom brick.

Xbox color queries now sample the full-resolution source texel with `floor`
and explicit image bounds. Alpha-only collision grids remain unchanged. This
also fixes a color boundary inside the image where reduced-grid rounding read
the neighboring color. Four one-pixel masked-reporter checks extend the fixture
to 38: the preceding pen-aware build returns PASSES=35 / ERRORS=3, while the
exact-pixel build returns PASSES=38 / ERRORS=0 on the identical archive.

The new normal-build XBE is
`b75e9beb9e910071dadb90654d66f90ef4e8570b313f3650fc2adaa7fbed475f`;
its matching symbols are
`08b51e5cf338a3e73c8245c3acf9d27bc6a38cbe47e7f2892d79e38811919bb0`.
The earlier `2141b1a0` build and symbols are preserved in
`build-xbox/pen-color-20261027/revision-2141`. New-build manifests, exact contact
coordinates, both edge-test counter snapshots and images are retained beside
it. Ordinary full-board play, stable scoring after clear, and physical Xbox
regression remain separate qualification gates.

### Combined normal-game check

The `b75e9beb` candidate with the prepared `a087d771` Breakout archive completed
an ordinary unseeded round through controller inputs in xemu. All five colored
bands cleared at Score 1800 with all three lives; three successive framebuffer
captures found no remaining brick-color pixels. The score then stayed at 1800
for another 600 input frames while the ball traversed Y=-154.65 through Y=176.
Across 15,269 observed frames, the warp eraser was never visible at a drawn
frame and no ball-position changes occurred during consecutive erasure-pending
frames. A faint one-pixel scaled boundary remains in the framebuffer; the
cleared field has no remaining brick collisions.

After releasing the bot, the round reached its original Game Over. The private
app's actual keyboard-controller Start hold, Down and A opened pause and
selected Green Flag. A read-only breakpoint captured Score 0, controller mode
0 and erasure-pending 0 after replay; native Right then moved the paddle to
X=201 and restored direct-key mode. Native Exit returned to the populated
browser. No positions, scores, lives or brick state were injected. Networking
initialization was bypassed under GDB only. These are emulator results; the
separate hardware candidate still needs its regression and gameplay checks.

### Physical Xbox regression

The same `b75e9beb` executable and identical 38-case fixture (`184676cb…`)
now pass on the physical 128MB Xbox: PASSES=38, ERRORS=0, FAIL CASE=0.
The temporary project launched from the normal browser and returned through
its regular pause/Exit path. The candidate remains in its separate test folder;
the regular dashboard executable has not been replaced. Screenshot and
full-hash receipt: ignored `pen-color-20261027/hardware-regression-38-pass.jpg`
and `hardware-regression-verification.json`. This proves the color regression,
not full Breakout gameplay or acceptance of all twenty games.
