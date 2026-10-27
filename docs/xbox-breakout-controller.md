# Breakout controller preparation

`scripts/prepare-breakout.py ORIGINAL.sb2 NEW.sb2` creates a separate copy of
LVRS's Atari Breakout and an adjacent controller profile. The original remains
untouched. It requires the [localized-stage monitor fix](xbox-sb2-stage-monitors.md)
and [bitmap text-layer fix](xbox-sb2-bitmap-layers.md) for the complete game.

D-pad/left-stick Left and Right select direct paddle control. Movement is eight
Scratch units per frame and stops on release. The paddle center is clamped to
±201 so its original 78-unit width stays on the 480-unit stage. Mouse input
retains the original behavior until the first directional press; Green Flag
restores mouse mode. Hold Start for the runtime menu, then choose Green Flag
to replay or Exit Project to return to the game list. The normal dashboard
combo remains available while the application is responsive.

The preparation adds one mode variable, a Green Flag reset and two key hats,
and replaces the single mouse-position assignment with a mouse/key branch.
Removing those changes reconstructs all original project data. All sixteen
assets remain byte-identical, and the ball, collision, scoring, life and ending
scripts are preserved.

Prepared archive SHA-256:
`98391a7b7c87bb1487d32edaec03a8415ae7e7e03f709b4403e90398db947d16`.
Profile SHA-256:
`edbea9a0d29318b806344ccc568b80f867575899bb2017998fff3d3a6cc3502b`.
Original archive SHA-256:
`944130addd4e809eeb992d6e4a5d6e9619d5d9c11af7026b238a348fd4a03c1d`.

## Xbox-executable checks in xemu

Using clean build `242d1c78…`, D-pad Right moved the paddle to the right edge
while two lives remained. Left-stick Left moved it across the stage. The run
reached Score 60 and the complete Game Over screen. GDB confirmed direct-key
mode and X=-201; a subsequent D-pad hold confirmed X=+201. The limit checks
continued after the authored ending, whose original paddle loop remains active.

Green Flag replay restored mouse mode. The later GDB observation showed Score
20 in the new round, so an exact zero-score framebuffer is not claimed. A new
Left press re-enabled direct control after replay, and held-confirmation Exit
returned to the populated browser. The project contains a sound asset but no
authored audio-playback blocks were found; silence is not an audio-backend
failure. This does not establish full-board completion.

Evidence is in ignored `build-xbox/breakout-controller/`, including
`controller-{left,right}.png`, corresponding stage/sprite JSON snapshots,
`controller-replay-movement.png`, `controller-exit-browser.png` and
`controller-gdb.log`. These emulator runs use the existing GDB-only network
initialization bypass. Hardware results follow below.


## Physical Xbox check, October 26

The original failure log ended after texture precaching. The prepared archive
and profile were installed with full FTP readback verification, preserving
`E:/ScratchEverywhere/Atari-original.sb2.bak`. Clean build `242d1c78…` was
installed separately in `F:/scratch-breakout-20261026/default.xbe`; the regular
dashboard launcher was not replaced.

Normal browser selection launched gameplay. The first run displayed Score 40
and two lives, then reached Score 100 and the complete Game Over message.
Left-stick Left moved the paddle left; D-pad Right moved it right. These first
captures span the ending, whose original paddle loop continues to run. Player
One subsequently moved the paddle during the replay round as well.

Several Start holds and a reset chord sent through the Player Three KVM route
did not recover the menu. Read-only route diagnostics showed delayed Player
Three adapter diagnostics while the other adapters were current. Switching to
Player One opened pause with a five-second Start hold. This supports an input
route problem for those failed attempts, rather than a proven runtime-menu
failure. Green Flag replay restored bricks and lives; the captured new round
already showed Score 20/two lives, not an exact zero-score frame.

Local evidence is under `build-xbox/hardware/`: `breakout-original-freeze.log`,
`breakout-deploy.json`, `breakout-fixed-playing-40.jpg`,
`breakout-fixed-stick-left.jpg`, `breakout-fixed-dpad-right-ending-100.jpg`,
`breakout-fixed-p1-pause.jpg` and `breakout-fixed-green-replay.jpg`.
Full-board completion is still unverified. No authored audio-playback blocks
were found, so silence is expected for this project.

Exit returned to the populated browser with A held for 2.2 seconds. Selecting
Breakout again launched a new round, with three lives/Score 20 visible during
left-stick movement. The second round later reached Score 40/Game Over.
`breakout-fixed-exit-browser.jpg`, `breakout-fixed-second-launch-stick.jpg`
and `breakout-fixed-second-dpad.jpg` preserve these observations. Full-board
completion remains pending.

The second held-confirmation Exit also returned to the browser and stayed there
(`breakout-fixed-second-exit-browser.jpg`).

## Brick-hit eraser preparation

The current preparation also repairs the authored eraser's grid selection.
The original MarkOut loop rounds the ball center to a 22-unit cell; shallow
hits can overlap a colored row while that center rounds to the row below.
The pen-aware build exposed this at Y=53.31, which selected eraser Y=44 below
the blue row at Y=56–78.

The prepared copy pauses the ball's movement while its existing brick-contact
branch broadcasts an erasure request. A warp procedure checks the five original
rows and the three nearby columns, stamping only original-size cells that
touch the ball and still contain the corresponding brick color. The probe is
shown and hidden within the warp procedure, because hidden sprites correctly
report no sprite contact. It is hidden during ordinary rendering. This keeps
the original ball speed, score increments, lives, collision bounce expression,
wait durations, backdrop and all sixteen assets. It requires the pen-aware
color-sensing engine repair; it does not turn every backdrop hit into a score.

Removing the movement gate and three erasure commands reconstructs the
original Ball sprite exactly. The life, death-zone, ending, intro and overlay
sprites are unchanged, as are the previous controller profile and paddle
limits. Preparation is deterministic, accepts only the tested original source,
and refuses an existing output/profile.

Prepared archive SHA-256:
`a087d771c3e45558af4da3cb5a6b99370bd9c862b5c67a873f9843c6a4cc525a`.
The earlier `98391a7b` preparation above contains steering changes only.

An ordinary controller run on the first pen-aware engine visually cleared all
five rows and retained all three lives. That run exposed the separate exact-
pixel sampling defect described in [color sensing](xbox-color-sensing.md), so
it is not a full functional pass. The exact-pixel engine then passed ordinary unseeded full-board play at Score
1800 with all three lives. Three successive captures showed all five bands
clear, followed by 600 stable-score frames crossing the empty field. No eraser
visibility or erasure-pending movement violations occurred across 15,269 frames.
Native keyboard-controller Green Flag replay reset Score/mode/pending to zero;
Right moved the paddle to X=201, and native Exit reached the populated browser.
Physical testing remains pending. Local archives, asset comparisons, input histories,
framebuffers and colored-pixel observations are under
`build-xbox/pen-color-20261027/`.
