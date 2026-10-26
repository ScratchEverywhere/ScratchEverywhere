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
