# Scratch 2 SVG measurements and Jumper

Jumper reached level three on the physical Xbox, but its floor and lava were
missing. The player repeatedly fell and respawned. The asset is intact: its
exported SVG says `viewBox="-1 5 482 57"`, while the embedded floor/lava bitmap
is translated to Y=193 and extends to Y=221. The exported box excludes it.
The rotated instruction on level one was also clipped.

Scratch 2 ignores these exported dimensions. Scratch's
[official SVG importer](https://github.com/scratchfoundation/scratch-svg-renderer/blob/develop/src/load-svg-string.js)
measures the complete drawing after text normalization, then replaces the root
width, height and viewBox. Its
[SVG skin](https://github.com/scratchfoundation/scratch-render/blob/develop/src/SVGSkin.js)
subtracts the new viewBox origin from the costume rotation center.

The SB2 image path now measures the root drawing in SVG coordinates with
LunaSVG, replaces the viewport, and exposes its crop origin. Runtime costume
loading applies the corresponding rotation-center difference, including cache
hits and clone copies. Retrying after a temporary missing-image fallback
restores the original center before applying the new offset. SB3 and UI images
do not enter this measurement path. LunaSVG's stroke bounds may conservatively
include extra transparent space around miter joins; they are not claimed to
match the reference importer's uniform stroke padding exactly.

## Validation

`python3 tests/scratch2_svg_bounds_test.py` uses native LunaSVG and Ryu with
address/undefined sanitizers. It checks off-viewport and negative geometry,
pixel colors, complete strokes, transforms, empty drawings, repeated cache
loads, clone origins and fallback/retry origins. All checks passed. Existing
`tests/scratch2_svg_test.py` text-conversion checks also passed.

The first Xbox candidate used `snprintf("%g")` for new SVG lengths. The pinned
NXDK C runtime leaves floating-point printf conversions unimplemented. GDB
found that crop offsets were calculated but raster dimensions stayed 481×57;
the floor remained absent. That candidate (`93ccc1d3…`) was discarded without
launching it on hardware. The corrected helper uses the same Ryu decimal
formatter already used by runtime numeric values.

The corrected regular-browser XBE has SHA-256
`1953d3d19148e5b139343553422202133b40a332aa793a5ba749aca116bcee9f`
(9,687,040 bytes). With the prepared Jumper project, A started gameplay and
jumped onto the level-two steps in xemu. Level three then rendered its floor
and red lava. GDB read back the level-three image at 640×290 pixels, scale
1.33333337, crop origin (0, 3.03199768), and adjusted center
(240, 37.96800232). The world drawing retains its original position. Evidence,
matching ELF/ISO and `platform-current.json` are in ignored
`build-xbox/jumper-controller/bounds-ryu/`. These emulator runs used the existing
GDB-only network initialization bypass; it is not compiled into the executable.

## Controller preparation

`scripts/prepare-jumper.py ORIGINAL.sb2 ORIGINAL.sb2.json NEW.sb2` creates a
separate archive and sidecar profile. A maps to Up, which the original player
script uses for ground jumping. One guarded stage key hat starts the existing
game when Up is pressed in its normal menu; it does not restart during play.
The original mouse Play and Cube Color controls remain available. The first
prepared archive (`3ca3fc33…`) exposed a fast-start race: A could broadcast
Start while the original 25-frame menu entrance was still moving sprites.
The preparer now adds a menu-ready variable, resets it on Green Flag/menu entry,
and sets it after Play’s authored entrance. An early A waits for readiness,
then clears it before broadcasting Start; Start/color-picker entry also clears
readiness. The new archive hash is
`48f4577fa315e8f915c7713f36dfa0dab859b16f987871f6cc3fd57ef351a62e`.
Removing these documented additions reconstructs all original project data,
and all 65 assets remain byte-identical. On hardware, the same launch → 500ms A → four-second Right sequence that
previously left menu geometry in gameplay now reached level two cleanly
(`jumper-ready-fast-start-level-two.jpg`). Two consecutive held-A exits remained
in the browser (`jumper-ready-{first,second}-exit-browser.jpg`). These runs used
the diagnostic XBE; the earlier extended gameplay observations below used the
first prepared archive. The prior intermittent Exit failure remains open.

Hardware baseline evidence includes D-pad right movement, Up jumping and
completion of levels one and two. An eight-second HDMI observation captured
7.668 seconds of signal, peak 0.12555/RMS 0.02874, without clipping in that
sample; this is not subjective or long-run audio acceptance.

The archive and controls have verified backups on the console as
`Jumper-original.sb2.bak` and `Jumper-controls.bak`. One overwrite upload
stalled with a 32MiB preallocated target. Only that staging process was stopped;
a fresh temporary upload/readback and rename recovered the archive and profile.
The discarded partial file is retained with `.bad` instead of a game extension.
The corrected XBE was staged separately in `F:\scratch-svg-bounds-20261026`,
fully read back, and launched for hardware verification. The regular Games
launcher remains the previous production build. Hardware terrain/control,
loss/replay, exit/relaunch and regression checks are still in progress.

The initial physical checks on that corrected candidate confirmed normal browser
selection and a title screen which waited for a fresh A press. A then started
the game. The full rotated “Arrow keys to move” instruction rendered, Right
completed level one, and A+Right jumped over the level-two steps. Evidence is
`build-xbox/hardware/jumper-bounds-{title,a-start-full-instruction,a-jump-clears-steps}.jpg`.

The corrected hardware run reached level three with the black floor and red lava
visible (`jumper-bounds-hardware-level-three-restored.jpg`). A bounded Right
hold and forty video observations captured repeated approaches to the lava and
immediate respawns above the left platform. The sequence is retained under
`build-xbox/hardware/jumper-lava-sequence/`; it establishes hazard death/recovery
without a reset or runtime restart.

A subsequent A+Right crossing sequence advanced to level four, whose wall-jump
terrain and restart instructions rendered (`jumper-bounds-hardware-level-four.jpg`).
The corrected gameplay audio observation captured 11.990 seconds at 48kHz,
peak 0.12543/RMS 0.02584, with no digital clipping in that sample.

Continued A+Right play cleared level four's wall-jump terrain and reached level
five's bitmap-based platforms, boost pad and lava wall. This also exercises the
existing bitmap-resolution path after the newly measured SVG levels.

Black/R restarted the active fifth level to its left spawn while retaining the
level and the ongoing run timer, matching the authored level-restart behavior
(`jumper-bounds-black-restarts-level-five.jpg`). The preparer also maps Y to G
for the authored G+R full-game restart. The final profile hash is
`454f1e88c619982febbd146a10301255c2ef7f2833758a3b1124474b44c7aa5a`.

Runtime pause → Green Flag, confirmed with A held 2.2 seconds, returned the
project to its title screen without carrying A into a new round. Evidence is
`jumper-bounds-green-flag-title.jpg`.

A fresh A press after that replay started level one again, with the captured
timer at 0.199999… (`jumper-bounds-replay-first-level-timer-zero.jpg`). This is
visible full-round reset evidence, separate from Black/R's level-only restart.

Pause → Exit Project with a held A returned to the populated browser, without
launching another project (`jumper-bounds-exit-browser.jpg`).

A fresh browser launch and A start again reached level two. Y+Black (G+R)
returned to level one and reset the timer, which had exceeded 72 seconds before
the shortcut. Evidence: `jumper-bounds-fresh-level-two-before-full-restart.jpg`
and `jumper-bounds-y-black-full-restart.jpg`.

The second Exit unexpectedly opened 2048, the first browser entry, instead of
remaining in the browser. This is a failed handoff check, not a second-exit pass
(`jumper-second-exit-unexpected-2048.jpg`). It is being investigated separately
from the restored terrain and gameplay; the regular launcher is not promoted.

The clean follow-up (`3ba0cfe9…`, menu tracing OFF) passed a physical Connect
Four diagonal win: thirteen alternating D-pad/right-stick drops formed the
expected board and visible Red Wins message. This exercises SB2 costume
geometry and stamped pieces after the crop adjustment. Evidence is retained
in `build-xbox/hardware/bounds-clean-connect-diagonal-win.jpg`.
