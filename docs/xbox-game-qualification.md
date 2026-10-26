# Ten-game hardware qualification

The goal is ten playable games, with actual Xbox testing and fixes until each
works. Downloads, successful builds, and a title screen are not full passes.
The current console has 128MB RAM; this does not establish retail 64MB support.

For each game, establish selection from the normal browser, correct visuals,
controller movement and actions, a meaningful gameplay objective/score, loss or
time expiry, replay, pause/exit, and another successful launch. Check collision
behavior where relevant and listen to audio before claiming audio acceptance.
Keep failures and missing checks explicit. Use bounded controller inputs and
neutralize all reports afterwards.

| Game | Evidence so far | Still needed |
| --- | --- | --- |
| Hopper Heros Everywhere | Browser launch/exit; movement and 6 points in earlier tests | Repeatable gameplay, loss/replay, audio |
| Ultra Bird | Flap, loss, embedded Start replay; browser launch/exit; 2 points | External replay shortcut, sustained play, audio |
| Catch Apples | Browser launch; expiry at score 7; short Start replay resets timer/score; pause and exit to list pass | Repeat launch, directed collection/collision, audio |
| Maze Starter | Browser launch/exit; movement down initial corridor | Wall collision, goal completion, replay, audio |
| Snake | Four stick directions proven by input/position trace; browser launch/exit | Clean D-pad/profile test, collection/scoring, loss/replay, audio |
| Appel v1.4 (griffpatch) | Empty-input clone fix + GPU address-range fix; visible terrain/scrolling/platforms; movement/jump; 10 apples; start/checkpoint respawn; quick restart; music/effects capture; pause/exit/relaunch | Full level; lwmem loading; long-run audio |
| Tetris (Tech-Coder) | Browser launch; D-pad movement/rotation, stick movement, A hard drop, top-out and Start replay | Line clearing/scoring, repeat launch/exit/audio; fix all-red pieces |
| Frogger (cs50) | SB2 profile fixed; D-pad/stick movement, collision/death, pause Green Flag restart, exit and repeat launch verified | Safe crossing/lily-pad scoring, audio |
| Atari Breakout (LVRS) | Packaged, uploaded and FTP readback verified | Mouse-emulation usability, all hardware gameplay checks |
| PacMan (cdaniels7088) | Packaged, uploaded and FTP readback verified | All hardware gameplay checks |

Sources, authors, instructions, packaging details and hashes for the additional
five are in ignored `build-xbox/more-games/next-five-manifest.json`. Game scripts
are unchanged; SB2 archives use numeric asset IDs assigned during packaging.
Upload readback results are in `next-five-installed.json` in that directory.
The browser visibly lists the new games; the first page shows Appel, Atari
Breakout, Catch Apples, Frogger and Hopper.

## October 25 continuation

Installed the multi-controller build in `F:\scratch-anypad-20261025`, SHA-256
`7059e259f086c5c6dd4503a14e36c4d0f997e2d8259d0fdd73380c82ef9a27cf`.
Player Three moved browser selection down to Hopper, Player One moved it back
up to Catch Apples, and Player Three launched Catch Apples. This verifies
switching between those two virtual controllers without relaunch. Physical-pad
switching and hotplug remain to be tested.

Catch Apples reached Timer 0 / Score 5. Holding Start opened the runtime pause
menu, so the reported freeze was not reproduced as an application hang in this
run. Its scripts intentionally stop all at timer expiry. A short-Start replay
shortcut is being tested to make recovery discoverable without the pause menu.
The next build, `F:\scratch-replay-20261025`, has SHA-256
`16895eb85396fbbe16142fde0baf05a2c6aad69b621eeed57e274f22465fd600`.
It also accepts array-valued control bindings, with arrow-game profiles installed
for Appel, Tetris, Frogger and PacMan. Those profiles map both D-pad and left
stick to arrows, and A to Space. Atari Breakout currently uses the existing
shoulder + D-pad mouse emulation and still needs a usability assessment.

With that replay build, Catch Apples reached Timer 0 / Score 7. A 250ms Start
press on Player One started a fresh round; the next capture showed Timer 25 /
Score 1. Holding Start on Player Three opened pause during that round, and A
exited to the browser. Evidence: `catch-replay-stopped.jpg`,
`catch-replay-after.jpg`, `catch-replay-pause.jpg`, `catch-return-list.jpg`.

Tetris was selected from the scrolled browser and launched. After top-out,
250ms Start produced an empty board with a fresh L piece. D-pad Up rotated it
to a vertical L and Player One's left stick moved it to the right wall. A
hard-dropped the following piece and a new piece spawned. This exercises array
bindings on the D-pad and stick, string bindings on A, and live controller
switching. Evidence: `tetris-selected.jpg`, `tetris-launch.jpg`,
`tetris-replay.jpg`, `tetris-rotation-stick.jpg`, `tetris-hard-drop-live.jpg`.

Some intermediate Tetris still captures were stale after the old 180-second
stream expired; they are not acceptance evidence. The capture helper was
extended to a persistent one-hour stream, and live status showed one viewer
and a frame age of 13ms before the replay/rotation/stick/hard-drop checks were
repeated. Keep a live stream and verify freshness during later tests.

All Tetris pieces render red. The project uses color effects; the Xbox renderer
currently does not forward/apply `Sprite::colorEffect` as the OpenGL core
renderer does. Treat this as an unresolved compatibility defect, not a full
game pass. No line-clear/scoring or audio acceptance is established yet.

The retrieved `build-xbox/hardware/replay-controls.log` confirms both arrow
bindings for every direction, `space -> A`, active-controller changes, two
`Xbox Start: restarted stopped project.` entries, and the reset-combo entry.
The final reset was sent from Player One and returned to dashboard FTP.

## Frogger and shared menu mappings

Frogger launched in the replay build, but D-pad arrows did not steer it.
The initial runtime log had no control-profile load. `loadSpritesSb2` did not
call `Input::applyControls`, unlike `loadSpritesSb3`; adding that call makes
SB2 projects use their installed sidecar profiles too.

With that change, D-pad Up moved the frog from Y=-170 through -160, -110 and
-60. The bounded trace confirmed `up arrow,any` with `alive=1`, then death at
Y=-20 with `alive=0`. The visible game-over screen reported about 6.8 seconds.
Evidence: `frog-profile-test.jpg` and `frog-profile-trace.log`. A score/lily-pad
pass has not been established. The optional Scratch sensor-board reporter
`sensorPressed:` is unsupported and logged; the main arrow steering still runs.

The profile's A-to-Space mapping also exposed a pause-menu defect: A did not
confirm Green Flag because the menu consumed the game's remapped keys. The
Xbox pause menu now saves the game mapping, uses default UI controls while
open, then restores the saved mapping on cleanup. In the clean build, A
confirmed Green Flag, and the frog returned to its start. A 600ms Player One
stick-left report moved it left on the starting strip; a Player Three D-pad
right report moved it back. This verifies A confirmation and mapping restoration.
Evidence: `frog-clean-restarted.jpg`, `frog-clean-stick-restored.jpg`,
`frog-clean-dpad-restored.jpg`.

Temporary position/thread tracing was removed from the clean build. The first
Player Three reset attempt while paused did not reboot; Player One's reset
did. Keep that as an unresolved intermittent reset observation rather than
claiming every-controller reset reliability.

The clean build in `F:\scratch-sb2-pause-20261025`, SHA-256
`2bf373581415ef505342188a47406de2e13d61fbe0437e128485b83c79a66734`,
had one real purple-screen browser startup (live frame age 1ms, flat for over
21 seconds). Reset recovered it. The same binary rendered the browser on its
next launch. `sb2-clean-purple.log` records successful browser activation and
no explicit rendering error. Startup reliability remains unresolved.

During active Frogger play, a four-second Start hold did not reliably open the
old pause menu. The implementation counted target-rate frames rather than
wall time. Xbox pause entry now uses a physical Start hold and a three-second
Timer, independent of game key remapping and actual frame rate.

The timed-pause build in `F:\scratch-sb2-timer-20261025`, SHA-256
`e9d7ee16fccbcebc6bbc1e5b686d47a04b7c9cb3c0e8b5b23c058d3047acfd08`,
rendered the browser and launched Frogger. An 800ms hold left the game running;
a four-second hold opened pause during active gameplay. Evidence:
`frog-pause-short-negative.jpg`, `frog-pause-timed-positive.jpg`.

A selected Exit Project, but the next game frame stayed visible. Captures 34
seconds apart were pixel-identical, live KVM input was still present, and
neither a further Start hold nor the Player One reset combo recovered it.
After physical dashboard recovery, `frog-exit-frozen.log` changed the diagnosis:
both cleanup calls finished, the browser activated, then another project
loaded immediately and stopped after preprocessing its roughly 3MB JSON.
That project is Appel, the first browser entry. The still-held A press was
reinterpreted as a fresh UI confirm when the game mapping was restored to
defaults. Thus the apparent cleanup hang was an unintended launch into Appel's
separate loading failure. Cleanup diagnostics were removed without deploying
them into a game. The final browser confirmation-release gate passed hardware
testing in `F:\scratch-sb2-release-20261025`, SHA-256
`87874a80f15ac4745e00195858007bd0662ebb779274ee67c57917b99c52fc58`:

- Catch Apples exited to the browser with A held for 1.2 seconds.
- Frogger, with its A-to-Space mapping, also exited with A held for 1.2 seconds
  and stayed in the browser instead of launching Appel.
- After release, a new selection and fresh A press launched Frogger again.

Evidence: `release-catch-return.jpg`, `release-frog-return.jpg`,
`release-frog-relaunch-selected.jpg`, `release-frog-relaunched.jpg`.
One shorter navigation sequence landed on Catch Apples rather than Frogger;
that launch was identified visually and excluded from Frogger results. Longer
600ms navigation presses were used, and selection was verified before launch.

The release-gate build also had a purple startup once and rendered the browser
after reset/retry. Preserve that separate startup issue for diagnosis. Appel's
2,995,120-byte archived JSON uniquely matches the accidental-load log. Its
loading failure remains unresolved; replacing it with a simpler game would not
satisfy the current ten-game qualification plan.

## Appel memory investigation

NXDK's CXBE tool sets the XBE initialization flag `bLimit64MB` (bit 2 at
header offset 0x124). Physical-memory detection still reports 128MB with that
flag, but the native allocator had only 53MB available at startup. Clearing
that flag raised startup availability to 117MB on this console. The packaging
step now clears only that bit, before producing the ISO; it can be disabled
with `SE_XBOX_ALLOW_EXPANDED_RAM=OFF`. Other header bytes were verified
unchanged, and the patched XBE was found byte-for-byte in the generated ISO.

Appel's authored MAP and MAPR lists each contain 6,400 entries. The constrained
JSON loader silently truncated them to 1,000. Xbox now parses complete lists
and avoids an additional JSON string copy when no Infinity sanitization is
needed. The native allocator build in `F:\native-expanded-fullmap-20261025`
(SHA-256 `d4a1c33318da7d93f28085102a9dbb71d2e34e8efcdf08e2178b5a3bf6b11274`)
parsed all 2,995,120 JSON bytes, cached textures and rendered level 1. Right
movement and A+Right jumping worked. After a longer jump test, the scene
became mostly empty sky; the runtime pause menu still responded. Do not count
this as a gameplay pass or assume that map loading resolves all defects.
Evidence: `appel-native-expanded.log`, `appel-fullmap-level1.jpg`,
`appel-jump-test.jpg`, `appel-after-jump.jpg`, `appel-pause-check.jpg`.

The corresponding lwmem build with the XBE cap removed remained on the browser
image after selecting Appel, including a second 600ms confirm attempt. The
Player One reset combo did not recover dashboard FTP. Its retrieved startup log confirms the heap stayed at 32MB despite reporting
128MB physical RAM; the expanded contiguous allocation did not grow the heap.
`SE_XBOX_TRACE_GAME_STATE=ON` enables an optional stage-variable snapshot when
opening pause, to distinguish gameplay state from rendering problems. It is
off by default.


The native state build (`3374be03d989f103bde13b93300b9d98d02f7cace8064848ac1e9f98f321ac7c`)
launched Appel again. Pause snapshots confirmed APPLES increased from 0 to 2
and GAME STATE changed from BEGIN to PLAY. The authored LEVEL # was 2; the
earlier description of the scene as level 1 was not verified. A subsequent
terrain trace confirmed MAP and MAP R remain 6,400 entries during play, but
there were no Foreground clones and the runtime had reached its 300-clone
limit. Clone population is being investigated. This does not yet establish
the cause of missing terrain. The terrain diagnostic exited to the browser
and the Player One reset combo returned to dashboard FTP. Evidence:
`appel-state-native.log`, `appel-terrain-native.log`,
`appel-native-exit-browser.jpg`.


## Appel clone and GPU fixes verified on hardware

GDB traced the clone overflow to an absent Boolean CONDITION returning without
initializing its output. That ran a deliberately disabled setup script in the
Levels sprite. Defaulting missing inputs to false/zero/empty restores the
expected menu population of 156, including 63 Foreground and 63 Background.
The 300-clone limit is unchanged.

The full-project Xbox test then exposed GPU allocations outside pbkit's DMA
range on expanded RAM. Constraining SDL XGU textures and the vertex arena to
MAXRAM restored visible browser/game rendering. The fixed native build
`01565e87aa3b86b0cb41ed2aac82e8140552b97b564f253f26b050c986996423`
ran Appel from the browser, entered the level labeled LEVEL 1, moved/jumped
across raised platforms and collected four apples with terrain intact.
The gameplay log confirms 168 live clones, including 63 Foreground; MAP/MAP R
were 16,384 entries after level setup. Pause and exit to the game list worked.
The empty-input regression also passed on Xbox (3 passes, 0 errors, sum 7).
See [the GDB investigation](appel-clone-debugging.md) for cause, controls,
artifact paths and remaining qualification work.

After returning to XBMC with the controller reset combo, the same fixed XBE
booted a visible browser again and launched the full Appel title screen.
Evidence: `appel-fixed-repeat-title.jpg`. The console was left on that title.


## Appel extended play and HDMI audio (October 25, 2026)

The same native GPU-range build reached ten apples, traversing both opening
checkpoints, ground spikes, an overhead spiked platform, blue weak platforms,
red enemies and the lower route under a floating spiked eye. Spike and enemy
deaths showed the breakup animation and respawned at the start or the activated
checkpoint. The retrieved `appel-death-and-audio.log` records APPLES=10,
LIVES=0, GAME STATE=PLAY, 167 clones and 63 Foreground tiles. This is extended
play evidence, not a level-completion pass.

The KVM HDMI PCM endpoint captured actual 48kHz stereo output. A 11.995-second
sample had peak 0.2494, RMS 0.008734 and zero reported capture overruns. Its
spectral pattern matches the bundled music (cosine similarity 0.723 versus
0.180 median at unrelated alignments). Sample correlation also identifies the
Player impact sound at 0.991 and jump at 0.780. Thus music and effects reach the
HDMI output without digital clipping in this sample; subjective listening and
long-run audio stability are separate checks. Evidence: `appel-death-audio.wav`,
`appel-death-audio.json`, `appel-audio-verification.json`, `appel-sfx-matches.json`
and `appel-blue-enemy-combined.jpg` in ignored `build-xbox/hardware`.

`examples/appel-xbox-controls.json` adds Y -> P (Appel pause) and Back -> Q;
Black already maps to R, so Back+Black supplies Appel's authored Q+R quick
restart. This profile is specific to Appel, preserving the generic arrow-game
profile for other games. Hardware verification passed: Y opened Appel's pause menu; Y resumed it;
Back+Black reset APPLES from 1 to 0, the timer to 0:00.0 and the player to
the start without rebooting. Evidence: `appel-y-pause-test.jpg` and
`appel-quick-restart-result.jpg`.

One profile-reload boot again rendered only clear colors, although its log
retained all 63 Foreground clones. Retrying the unchanged XBE restored browser
and Appel rendering. Startup rendering remains intermittently unreliable.

Quick restart also passed after a subsequent spike death: APPLES=0,
timer=0:00.0 and the player returned to the start, then Y opened the game pause
menu. The console was left there, safely paused; Y resumes. Evidence:
`appel-restart-after-death-paused.jpg`. A further 9.995-second HDMI recording
after restart contained nonzero game audio (peak 0.1465, RMS 0.006139), with no
reported capture overruns (`appel-after-restart-audio*.json/.wav`). First-level
completion remains unverified: the best attempt collected ten apples and
passed both opening checkpoints before repeated deaths. No completion screen
or completed-level replay is claimed.


## Startup rendering reliability repair

The intermittent purple-browser/black-game startup was isolated with GPU pixel
readback: clears worked, but fixed-function vertex processing could produce no
visible geometry. Switching the same failing boot to an explicit vertex
program immediately restored solid and textured drawing. Position defaults,
clipping/test resets and vertex-cache invalidation did not repair the failure.
The renderer now initializes a screen-space vertex program and its viewport
offset explicitly.

The final GPU regression probe passed solid/textured rendering, viewport
translation/clipping, render-target restoration and alpha modulation. With both
renderer and gameplay diagnostics disabled, the installed normal launcher then
passed **eight consecutive dashboard launches**, each with a further 6.2-second
visible-browser stability check. The eight screenshots were reviewed together.
This covers warm launches on the 128MB console at 480p; it does not claim eight
physical power cycles.

The verified executable is `F:\Games\ScratchEverywhere\default.xbe`, SHA-256
`3e68d3a127afdd2fe0ac99d74c54177ea5956e9b40ef59eaf7f6898d92b95fe1`.
It continues to read the existing projects from `E:\ScratchEverywhere`.
See [the startup investigation](xbox-startup-rendering.md) for controlled
experiments, the optional diagnostic build flag and proof artifact paths.

On that same installed production build, Appel subsequently passed title and
level rendering, right movement, jumping, collection of one apple, and runtime
pause → Exit Project → visible browser without rebooting. Evidence:
`startup-final-appel-{title,level,moving,exit-menu,browser-return}.jpg`.
