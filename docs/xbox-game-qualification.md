# Twenty-game hardware qualification

The goal is twenty playable games, with actual Xbox testing and fixes until each
works. Downloads, successful builds, and a title screen are not full passes.
Twenty distinct projects are now packaged and installed with archive readback
verification. Installation is not qualification; the additional ten still need
the gameplay checks below. The current console has 128MB RAM; this does not
establish retail 64MB support.

For each game, establish selection from the normal browser, correct visuals,
controller movement and actions, a meaningful gameplay objective/score, loss or
time expiry, replay, pause/exit, and another successful launch. Check collision
behavior where relevant and listen to audio before claiming audio acceptance.
Keep failures and missing checks explicit. Use bounded controller inputs and
neutralize all reports afterwards.

| Game | Evidence so far | Still needed |
| --- | --- | --- |
| [Hopper Heros Everywhere](xbox-hopper.md) | Normal-build browser launch; added missing D-pad/arrow profile without archive changes; D-pad/stick Left/Right and A jump; post-profile losses at 3 and 8 points, Start replay to zero, B to authored menu, browser returns and fresh-launch gameplay/movement; 8s non-clipping HDMI audio | One early short Start missed after relaunch; sustained progression and subjective/long-run audio; shared KVM handoff |
| [Ultra Bird](xbox-ultra-bird.md) | Current normal-build browser selection; A flap, loss, Score 2 and another Score 1; short Start restarts a scored stopped round to zero with new moving pipes and another A flap; browser return and fresh launch with two more A flaps; long Start opens pause; original archive has no sound assets or sound/music blocks | Sustained flight beyond two scored pipes; shared KVM delivery |
| [Catch Apples](xbox-catch-apples.md) | Fixed missing D-pad profile; synchronized build D-pad/stick steering, directed collection to Score 22/Timer 0, short Start replay to 0/30, pause/exit, fresh launch and second exit; 8s non-clipping HDMI audio | Intermittent exit launches 2048; isolated bonus/rotten-apple scoring; subjective/long-run audio |
| [Maze Starter](xbox-maze-replay.md) | D-pad/stick profile; repaired color wall collision; prepared one-frame Goal startup yield fixes stale win text; three full-route wins across traced and normal builds, three clean Green Flag replays, browser return and fresh launch; no authored sound blocks/assets | Shared intermittent browser handoff remains open |
| [Snake](xbox-snake.md) | Four stick directions in prior trace; synchronized build browser launch, sustained D-pad movement in all four directions, fruit collection/Score 125 and loss screen, Green Flag replay to later Score 0, pause/exit, fresh browser launch with four D-pad directions and second exit; 8s non-clipping HDMI audio | Explicit self-collision, clock/blade pickups, subjective/long-run audio |
| Appel v1.4 (griffpatch) | Empty-input clone fix + GPU address-range fix; visible terrain/scrolling/platforms; movement/jump; earlier 10 apples; current normal-build route reaches 7 apples and checkpoint two; diagnostic controller run reaches both moving lifts, 8 apples and the roof beyond them; death/respawn; quick restart; music/effects capture; pause/exit/relaunch; live pause snapshots verified | Full first level beyond the roof after checkpoint two; lwmem loading; long-run audio |
| Tetris (Tech-Coder) | Hardware color/pen fix; normal browser launch, movement/rotation/hard drop; one line and Score 40; top-out, Start replay to 0, pause/exit and repeat launch; 7.99s non-clipping HDMI audio | Subjective/long-run audio; full game win |
| [Frogger (cs50)](xbox-frogger.md) | SB2 profile fixed; D-pad/stick movement, collision/death, pause Green Flag restart, exit and repeat launch; road crossing/platform carrying; prepared prompt seven-pad win and ordered replay initialization pass targeted xemu collision probes; prepared copy passes hardware movement, traffic death/replay, two launches/two exits; 8s non-clipping HDMI audio | Complete river crossing/lily-pad scoring and seven-pad win through controller play on hardware; subjective/long-run audio |
| Atari Breakout (LVRS) | Localized stage monitor crash and bitmap text layers fixed; prepared D-pad/stick controls; hardware browser launch, Score 100, life loss/full Game Over, Green Flag replay, held-confirmation exit and second browser launch; Player One pause works after delayed Player Three KVM route | Full board; no authored audio-playback blocks |
| PacMan (cdaniels7088) | Browser selection/launch; turns/walls; 70 points before name fix; repaired Game Over; Start replay to Score 0, then 20 points; pause/exit; no authored sound playback | Power-pellet/ghost interaction, board win, repeat launch on combined build |
| Space Invaders (RokCoder) | Hardware browser launch; D-pad left/right, A start/fire, Score 450, shield damage and life loss; A replay to 0/three lives; pause/exit, verified fresh launch and second exit; HDMI audio signal | Full wave; subjective/long-run audio |
| Asteroids (Will_Wam) | Hardware browser launch, thrust/turn/fire, 9,200 points and game over; repaired SB2 ending text; pause Green Flag replay to 0; exit, fresh browser relaunch and second exit; captured HDMI audio | Subjective/long-run audio; further level progression |
| Sokoban (Gammastr20) | Hardware completion of puzzles 1–3; four D-pad directions, crate/goal updates, Black/R puzzle restart; 7.995s HDMI audio; hardware pause/exit on the new candidate | Repeat launch; later puzzles, including authored level-10 dead crate; subjective/long-run audio |
| [2048 (JereTheJuggler)](xbox-2048.md) | Browser launch; four directions; extended play to 1024/Score 14456; new 631-command hardware trace confirms duplicate Down input at Score 11700, with matching legal grid/score; prepared A victory acknowledgement passes seeded xemu and hardware tests; normal prepared game reaches natural Game Over/Score 260, A replays to 0 and Up scores 4; clean reboot/browser launch, movement and short-confirmation Exit pass | Intermittent KVM missed/duplicate delivery and held-confirmation browser handoff; full normal-play win; no authored sound playback |
| Retro Racing (griffpatch) | Hardware browser launch, car/road/HUD/map, acceleration to 108 MPH, A braking to 27, D-pad/stick steering, off-road slowdown, Green Flag replay, two launches/two exits and controller dashboard reset; 8s non-clipping HDMI audio; xemu clone count 191 across replay and zero after exit; audio-fix build reaches 100 MPH, Y pause/resume and Green Flag to zero pass, extended driving progresses around opening bend | Full lap/traffic/drift; subjective/long-run audio |
| Hill Climb Racing (griffpatch) | Hardware jeep Score 1,000, motorcycle Score 420/+200 airtime; crash and fuel loss; A replay, X vehicle swap, active Black/R restart; repaired full messages; pause/exit and repeat launch; HDMI audio signal | Extended terrain, precise brake/air control, fuel pickup, authored Y pause, subjective/long-run audio |
| Super Mario Bros. (Will_Wam) | Archive uploaded/readback verified; arrow/Space profile; hardware startup fault reproduced in GDB and calendar reporter fixed; xemu movement/jump, block hit and Green Flag; fixed hardware launch, movement/jump, first two pipes crossed and browser exit | Diagnose return to start near third pipe; level completion, loss/recovery, repeat launch and audio |
| Doodle Jump (xamuil2) | Hardware browser launch, left/right platform steering, at least 1,044 m; fall and automatic replay; pause/exit, fresh launch and second exit; HDMI audio signal | Edge wrapping, monsters, extended play and subjective audio |
| Jumper (Depicklator) | Fixed SVG bounds; hardware A-to-play/jump, levels 1–4 cleared, level 5 rendered, lava death/respawn, Black level restart, Green Flag full replay, fresh launch and Y+Black full restart, browser exit and HDMI audio signal | Intermittent second Exit launches 2048; new readiness guard passes quick A start and two subsequent exits; later levels, subjective/long-run audio |
| Connect Four (aaronzoll) | Archive uploaded/readback verified; arrows/WASD profile; computed-list loader fix passes 19 checks in both engines; prepared initial state/monitor repair; hardware both control sets/colors, stacking, vertical/horizontal/diagonal wins, full-column rejection, Green Flag replay and repeated browser launch; exit | One initial mouse-mode launch remains unexplained; no authored sound-playback blocks |

Sources, authors, instructions, packaging details and hashes for the additional
five are in ignored `build-xbox/more-games/next-five-manifest.json`. Game scripts
are unchanged; SB2 archives use numeric asset IDs assigned during packaging.
Upload readback results are in `next-five-installed.json` in that directory.
The browser visibly lists the new games; the first page shows Appel, Atari
Breakout, Catch Apples, Frogger and Hopper.

## Appel controller route follow-up — October 26, 2026

The normal `07f15fc6…` build reached seven apples and the second checkpoint
through ordinary D-pad/A input. The matching terrain has the deep pit with
vertical spike walls behind the flag and moving blue platforms ahead. Death
returned the player to that flag with seven apples retained. Local captures
`appel-level1-deep-pit-jump.jpg` and `appel-level1-blue-attempt-respawn.jpg`
show those states. The original archive's decoded Level 1 map places this flag
at tile `(36, 3)`; that map reading is route context, not a live position trace.

Earlier captures named `checkpoint2-jump`, `checkpoint2-respawn` and
`middle-safe-tile` actually showed returns to the first checkpoint and are
excluded from the second-checkpoint claim. Attempts at the blue platforms
remain incomplete; an authored pause obscured the landing in one attempt.
The [optional live log mirror](xbox-network-logging.md) now exposes existing
runtime-pause position and checkpoint snapshots without returning to FTP.
Neither those snapshots nor this route establish a complete first level.

The subsequent diagnostic run used the same original archive, SHA-256
`4affb3db57373d2b301e0ce7d405971451a9adff62c936ec7092fc6964f1260b`,
and XBE `debed504…` from the live-logging checks. Ordinary controller play
reached checkpoint two again, crossed both moving blue lifts, collected an
eighth apple, and landed on the roof beyond them. A temporary host helper
timed jumps from fresh HDMI frames while the target lift descended. It sent
controller reports; no debugger or game-state writes were used.

| Live pause snapshot | PLAYER X | PLAYER Y | APPLES | SPAWN IDX | PLAYER DEATH |
| --- | ---: | ---: | ---: | ---: | --- |
| Checkpoint two | 2220.1045 | 197.01 | 7 | 421 | empty |
| First moving lift | 2346.1524 | 339.01 | 7 | 421 | empty |
| Second moving lift | 2490.2296 | 497.01 | 8 | 421 | empty |
| Roof beyond lifts | 2693.6018 | 497.01 | 8 | 421 | empty |

`PLAYER SY=0` in those landing snapshots. Captures
`appel-gated-ride2.jpg`, `appel-gated-lift2-ride.jpg` and
`appel-gated-roof-jump.jpg` accompany the ignored
`build-xbox/network-log/appel-route1.jsonl` and labeled snapshot JSON files.
The roof-jump image is airborne; the later pause snapshot establishes landing.
The next roof attempt hit an enemy (`PLAYER DEATH=1`) during pause/resume.
Earlier failed lift attempts reached its side (`PLAYER WALL=1`), fell onto
spikes, or wall-jumped back toward the checkpoint. These failures are retained
and do not count as successful crossings. The complete first level, later
checkpoint and finish remain unverified.

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

At this test point, all Tetris pieces rendered red. The project uses color effects; the Xbox renderer
did not forward/apply `Sprite::colorEffect` as the OpenGL core
renderer does. Treat this as an unresolved compatibility defect, not a full
game pass. No line-clear/scoring or audio acceptance is established yet.

A subsequent [color/brightness/pen repair](xbox-color-effects.md) passes 1,756
host math cases, fourteen GPU probes and 107/107 fixture pixels in xemu. It also
fixes alpha compositing through the pen layer. This candidate is not yet on the
physical console; colored Tetris pieces, line clearing, gameplay/audio and
browser exit/relaunch still need hardware verification.

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

## Additional ten-game installation

The new ten are listed in [the source/hash/control manifest](xbox-additional-games.json).
Their archives and profiles were uploaded to `E:\ScratchEverywhere` and fully
read back. ZIP integrity and unchanged game-script comparisons passed for all
ten; downloaded assets were checked against their original MD5 names. SB2
assets were assigned standard numeric archive IDs. Connect Four's absent pen
layer (`penLayerID=-1`, unavailable asset URL) was not included; all actual
costumes and sounds were included, and scripts were preserved.

The base arrow profile is adjusted only where needed: Y mutes Asteroids and
Sokoban; Y pauses the racers, Back mutes them, X changes Hill Climb vehicles;
Connect Four maps its second player's WASD to the right stick. Conflicting
base bindings were removed so every physical button has only one key. These
extra actions still need game-specific hardware checks.

Bomberman's cloud-multiplayer-centered version was screened out in favor of
Will_Wam's local Super Mario first-level remake. The original Geometry Dash
API returned 404 and was not substituted with an unverified third-party copy.
Installation is not a claim that all twenty games are qualified.

## Color sensing and Scratch 2 backdrop follow-up

The [color-sensing investigation](xbox-color-sensing.md) records restored PNG/SVG
CPU colors, conservative per-query culling, and the odd-sized collision-mask
edge fix. The final color build passed 14 regression cases twice on hardware.
Maze completed its full route with wall rejection, and fresh project relaunch
cleared its old win message. Green Flag replay reset its position but retained
the old message at that point. A later [prepared-copy startup repair](xbox-maze-replay.md)
yields before Goal checks Ball, preventing the win bubble from being recreated
before Ball resets. The later verification is summarized in the matrix.

PacMan's apparent freeze after Score 20 was a stopped project: holding Start
still opened pause, and short Start then reset Score to 0 while retaining High
Score 20. Its Game Over backdrop had not appeared. The SB2 parser did not fill
`costumeNameIndex`, while SB3 did, so all costume/backdrop switches by name could
silently do nothing. The SB2 parser now fills the same first-name lookup. The
`examples/make-sb2-costume-test.py` six-case fixture checks named Play/Lost backdrops, named red/green
sprite costumes and existing numeric selectors. The fourteen-case SB3 color
fixture is also rerun on the combined production build.

The combined production build passed **PASSES=6, ERRORS=0** in the SB2 fixture,
with the expected blue backdrop and red final sprite, and **14/14** in the SB3
color fixture. Evidence: `sb2-names-explicit-result.jpg` and
`combined-color-regression.jpg`. An earlier bundled navigation sequence stayed
on the previous color fixture; its `sb2-names-regression.jpg` is excluded from
SB2 acceptance. The test was then explicitly selected in the browser before
launch. The combined executable was read back from the normal Games folder,
SHA-256 `60dc7891023a9c476d1f5f19dd943ade8e1abbdd551a91aa2dd34eb29a77df57`.

The combined build also repeated Maze's first-wall rejection after a five-second
right hold (`maze-final-shared-build-wall.jpg`) and could exit to the browser.
This run resulted from a menu navigation landing one entry above PacMan; its
captures were renamed to Maze and excluded from PacMan results.

PacMan then launched from a visually confirmed selection on the combined build
and reached its actual red GAME OVER backdrop after a ghost loss. The crossed-out
yellow character matched its authored backdrop, with the normal maze and dot
sprites hidden. Evidence: `pacman-combined-start.jpg` and
`pacman-combined-loss-observation.jpg`. This directly verifies the named-backdrop
repair in a real SB2 game.

After that ghost loss, a short Start press restored the normal maze and Score 0
while preserving High Score 10. A subsequent right-input check reached Score 20;
a long Start hold opened pause and A returned to the browser. Evidence:
`pacman-combined-short-start-replay.jpg`, `pacman-combined-replay-score.jpg`,
`pacman-combined-pause.jpg` and `pacman-combined-browser-return.jpg`.
The original scripts have no audio-playing opcodes; its otherwise unused sound
assets do not imply missing music. A bounded 9.995-second HDMI capture was quiet,
consistent with that source (`pacman-gameplay-audio.wav`). Power pellets/edible
ghosts and board completion are not yet qualified.

Both temporary regression archives were then retrieved and compared with their
local originals before deletion from the console. All twenty selected game
files remained present with their expected sizes; the combined production XBE's
full readback still matched the hash above. Evidence:
`twenty-game-installed-final.json` and `combined-color-sb2-pacman.log`. The normal
browser was relaunched from `F:\Games\ScratchEverywhere\default.xbe`.

## 2048 long run and score audit

On the combined production build, 2048 launched from the normal browser and
accepted all four directions. Controller-driven play reached a 1024 tile in one
round. Four saved chunks contain 802 board observations, including 788 changed
boards; the observer uses HDMI pixels and ordinary controller reports, without
changing game state. The original installed archive was unchanged.

A later Up command produced a persistent board that did not match the observer's
one-move prediction. Pause/resume did not change it. A subsequent Down showed
Score 14456. This initially suggested a grid mutation defect, but the score
audit changes that interpretation: the pre-Up score inferred from saved moves
is **14428**, not the initially assumed 14432. The initial score was 12; visible
anchors at 1772 after the first chunk and 14416 at the last chunk's turn 90 match
that accounting.

One Up should gain 16 and reach 14444. The observed 14448 board matches **two
legal Up moves**: first spawn a 2 in the fourth column below the moved 2, then
merge those twos on another Up (+4), then spawn a 2 in the bottom row's third
column. Either of two first-spawn positions gives the exact observed board and
score. This is an inference supporting duplicate input; controller/USB edges
were not traced during the event. It does not prove the source of an extra
move, nor exclude every other cause.

Two isolated xemu/GDB seed runs each completed **1,000 iterations, zero errors**
using the original Up and random-spawn procedures. Each verifies the moved grid,
score gain, unchanged occupied cells and exactly one new 2/4 in an empty cell.
One seed uses numeric tiles; the other uses string 0/2/4 values, as in the game's
spawn scripts. The added diagnostic driver seeds the saved board and auto-starts
the game; networking is skipped and the SB2 loader format is selected in GDB for
the embedded bundle. Only the added driver's screen-refresh suppression was
changed to accelerate the test. Its initial score is explicitly set to 14432,
so its expected score 14448 checks the same +16 gain; it is not a claim that the
real pre-event score was 14432.

These are procedure diagnostics, not ordinary gameplay qualification. No grid
code was changed on this evidence. Win/loss, replay, exit/relaunch and the input
edge investigation remain open. The original project has no sound-playing
opcodes. The physical console subsequently became unreachable during the
separate startup follow-up, so hardware testing is waiting for recovery.

Ignored evidence: `build-xbox/hardware/2048-play-run{1,2,3}-results.json`,
`2048-play-results.json`, their saved images, `2048-second-stop.json`, and
`2048-mismatch-down-diagnostic.jpg`; `build-xbox/xemu-debug/2048-numeric-seed/`,
`2048-string-seed/`, `2048-source-up-result.json`, and
`2048-duplicate-input-hypothesis.json` preserve the diagnostics, snapshots and
score/move calculation. The seed archives were never uploaded over the game.

The normal browser candidate containing the color/pen fixes was then built with
no embedded fixture and with renderer/game tracing and debug symbols OFF:
9,478,144 bytes, SHA-256
`78d2fe178ae621a7d591d08af8439d829a49536686557f462eeb3df7d1788df5`.
Its XBE initialization flags are `0x1` (expanded RAM permitted). It is staged
locally as `build-xbox/hardware/color-effects-production-candidate.xbe`; it has
not replaced the installed `60dc7891…` production build.

The unmodified normal candidate also booted in the isolated emulator with NAT
networking, without GDB overrides. Its empty project browser rendered correctly
(`build-xbox/xemu-debug/production-candidate-startup.png`); the private HDD has
none of the hardware console's game archives. This is a launcher smoke check,
not a twenty-game hardware pass.


## Color build hardware gameplay (October 26)

The normal browser candidate with color/brightness/pen-alpha changes was
uploaded to `F:\scratch-color-production-20261026\default.xbe`; its complete
FTP readback matched SHA-256
`78d2fe178ae621a7d591d08af8439d829a49536686557f462eeb3df7d1788df5`
(9,478,144 bytes). Renderer/game tracing and debug symbols are disabled.
The normal Games launcher was not replaced for this test.

Tetris was visually selected from the normal browser and launched. Falling and
settled pen-stamped pieces showed distinct cyan, yellow, orange, green, red,
blue and purple colors. Bounded controller play cleared a row and the visible
HUD reported LINES=1, SCORE=40 (`tetris-ai-05-0.jpg`). The observer stopped when
it could not identify a fully visible falling piece, and later stopped on a
placement mismatch rather than assuming every report had been sampled. These
observer limits are not game failures or additional line-clear passes.

A deliberate stack of hard drops reached the authored top-out/thumbnail state.
A 250ms Start press restarted to LINES=0, SCORE=0 with a fresh falling piece.
A five-second Start hold then opened runtime pause, and A/Exit Project returned
to the visible browser. Evidence is
`color-production-tetris-{topout,replay,pause-after-replay,browser-return}.jpg`
in ignored `build-xbox/hardware`.

A 7.99-second, 48kHz stereo HDMI recording contained audio (peak 0.551, RMS
0.01687; no digital clipping in this sample). Matching against the bundled
Tetris theme yielded only 0.165 full-window sample correlation and 0.396–0.609
for one-second windows. Preserve those measured limits; do not claim exact
sample matching or subjective audio acceptance. Evidence is
`tetris-color-audio.wav`, its JSON, and the two audio-match reports.
A second visually confirmed Tetris browser selection relaunched the game
(`color-production-tetris-relaunched.jpg`). Pause/Exit Project returned to the
visible browser again (`color-production-tetris-repeat-exit.jpg`).

The same production candidate then launched the full Appel title and rendered
level four terrain. Right/up input moved and jumped the player; the five-second
Start pause and Exit Project returned to the visible browser. This checks basic
Appel compatibility with the color/pen changes, not level completion or a new
apple-collection pass. Evidence is
`color-production-appel-{title,selected-game,level4-moving,pause,browser-return}.jpg`.

The tested candidate was subsequently promoted to the regular Games launcher,
with the previous executable retained for rollback. See the
[color-effects hardware record](xbox-color-effects.md) for hashes and paths.


## Sokoban hardware progression (October 26)

On the installed color/pen build (`78d2fe17…`), normal browser selection
launched Sokoban. Up pushed a crate onto a goal and changed its costume/color.
Black/R restored the initial crate and player positions; the cumulative move
counter intentionally remained 1, matching the authored Load Level routine.
D-pad solutions Up/Down/Left, Up/Down/Down/Up/Left/Right/Right and Right/Up/Left
then completed puzzles one, two and three. The visible counters reached
Moves=14, Level=4, consistent with the earlier single push and restart.
Inputs were released between moves and spaced to respect the authored cooldown.
Evidence is in ignored `build-xbox/hardware/sokoban-hardware-*.jpg` and
`sokoban-hardware-three-puzzles.log`.

An HDMI sample contained 7.995 seconds of 48kHz stereo audio, peak 0.03030 and
RMS 0.003990, without digital clipping in this sample. This establishes signal
capture, not subjective or long-run audio acceptance.

A source-level check found a later authored obstacle to full completion:
level 10 starts with a non-goal crate at zero-based (12,2), with walls immediately
right at (13,2) and below at (12,3). Without pulling, it cannot leave that corner:
left/up pushes require standing on those walls, while right/down destinations
are walls. This is in the downloaded project data; it is not attributed to the
Xbox runtime. The level also has eleven goals, so leaving this crate trapped
prevents completion. It needs an explicitly documented project repair and
runtime verification before a full-game pass. Solver timeouts on other levels
are not evidence that those puzzles are impossible. Level 12 also has seventy
tile digits despite a 7×9 header; no runtime defect is inferred from that alone.

## Upstream runtime hardware checks and SB2 text (October 26)

The upstream candidate (`f90ec7b0…`) was uploaded and read back in the separate
`F:\scratch-upstream-20261026` folder. The regular Games launcher remained
`78d2fe17…`. Appel reached five apples in level one with visible terrain and a
checkpoint, and returned to that checkpoint after deaths. Back+Black held for
1.2 seconds reset apples and the timer to zero; a preceding 250ms attempt did
not visibly restart. Physical Start held five seconds opened the runtime pause
menu, and A exited to the populated browser without launching another project.
A full level and long-run audio remain outstanding. The captured HDMI sample
contained 7.995 seconds of signal without digital clipping (peak 0.14453).
Evidence is in ignored `build-xbox/hardware/upstream-appel-*.jpg`,
`upstream-appel-audio.json` and `upstream-hardware-candidate.json`.

Asteroids launched from the same browser. D-pad thrust/turning and A firing
were exercised, the score reached 2,350 and all lives were lost. Pause → Click
Green Flag reset the score to zero and restored the three reserve ships;
subsequent play reached 700. Pause → Exit Project returned to the browser.
Short Start did not replay the game-over scene: the authored project still has
running scripts, so it does not meet the stopped-project shortcut condition.
The Green Flag action is the available replay path. An eight-second audio
sample contained signal, peak 0.12561/RMS 0.01870, without digital clipping;
subjective audio acceptance is not established.

Asteroids also exposed misplaced ending text: “Game Over” was offscreen and
“Score” appeared at the right edge. Its original SB2 SVG includes text x/y
coordinates that Scratch 2 ignores, and a multiline message separated by lone
carriage returns. The image loader now normalizes original SB2 text before
LunaSVG rasterization, following the text positioning, baseline, font spacing,
inset and line-break rules in Scratch's
[official converter](https://github.com/scratchfoundation/scratch-svg-renderer/blob/develop/src/load-svg-string.js).
Project-format gating excludes SB3 assets and application UI. Project archives
and scripts are unchanged; this does not implement every other SB2 SVG quirk.

`python3 tests/scratch2_svg_test.py` passed with address/undefined sanitizers.
The corrected ending asset was also rendered with host LunaSVG 3.5.0 and the
bundled font, then compared with an independent XML implementation of the
reference conversion: both PNGs have SHA-256
`fb14fcf12b0e56f5ec66ca2e9469b408a996aac86fc7c09b7b40609eee42ee6d`.
The NXDK build succeeded. The separate hardware candidate is
`F:\scratch-svg-20261026\default.xbe`, 9,637,888 bytes, SHA-256
`dc10c0781f34cf8e723602f24046b671a89dbc57de01a0f0e08ce7495cd4425a`,
with FTP readback verified. The regular Games launcher hash was checked and
remains unchanged. The KVM restarted during the launch video mode transition,
then reconnected to a live populated browser. On hardware, the corrected ending and both instruction lines rendered after
a 9,200-point round. Pause → Green Flag reset the score to zero; Exit Project
returned to the browser. Selecting Asteroids again produced another fresh
zero-score round, and a second exit also succeeded. Evidence includes
`scratch2-svg-asteroids-ending-live.jpg`, `scratch2-svg-asteroids-replay-zero.jpg`,
`scratch2-svg-asteroids-browser-return.jpg` and
`scratch2-svg-asteroids-relaunch.jpg`; see `scratch2-svg-validation.json` and
`scratch2-svg-deploy.json` for the artifact record.


## Space Invaders hardware progression (October 26)

On the SB2-text candidate (`dc10c078…`), Space Invaders was selected in the
normal browser and launched. This is an SB3 project after an SB2 game; the
playfield, title, score, shields and invaders rendered normally. A starts a
round and fires, and the D-pad moved the base left and right. A firing sweep
reached Score=450, with visible enemy removal, shield damage and life loss.
A remained held across the first loss and the game had already started a new
zero-score, three-life round by the next capture. In a subsequent round with
neutral input, lives reached zero and the authored “Press SPACE to play”
title returned. A then started another round at zero with three lives.

Physical Start pause and Exit Project returned to the populated browser. A
fresh launch was repeated with selection confirmed before A; the title and
a fresh zero-score, three-life round both rendered, then a second exit
succeeded. A preceding 350ms navigation sequence had landed on Sokoban and
is excluded from the Space Invaders relaunch result. The repeated selection
used 550ms presses and 350ms release gaps. The incidental Sokoban launch did
render its level-one puzzle and successfully pause/exit on this candidate,
but did not repeat its puzzle progression. Short virtual-controller presses
still need separate input timing investigation before attributing misses to
the game or runtime.

The 7.990-second HDMI audio sample had peak 0.06738 and RMS 0.008795, with
signal and no digital clipping. No full wave was cleared, and subjective or
long-run audio acceptance remains outstanding. Evidence is in ignored
`build-xbox/hardware/scratch2-svg-space-*.jpg`, `scratch2-svg-space-audio.json`
and the `space_invaders` section of `scratch2-svg-validation.json`.


## Doodle Jump hardware progression (October 26)

On candidate `dc10c078…`, the normal browser launched Doodle Jump. Bouncing
is automatic; D-pad left/right steered between platforms and the visible
height reached at least 1,044 m. A deliberate move away from the platforms
produced a fall, showed the high-score monitor and started another round
automatically. The project source confirms its outer loop waits two seconds,
deletes old clones and resets its round variables. In the repeated live-video
check, `doodle-verified-replay-23.jpg` shows the 278 m loss state and
`doodle-verified-replay-28.jpg` shows the new round at 35 m. This is visible
reset evidence, not a claim that an exact zero frame was captured.

A five-second physical Start hold opened runtime pause, and A exited to the
populated list. Selection and launch were repeated, followed by a second
successful exit. The old bounded video-reader session ended during later
review; replay/launch/exit were repeated with a new reader. Live-status records
show one viewer and frame ages of 10/11 ms for those checks. The eight-second
HDMI sample contained signal, peak 0.02960/RMS 0.004464, without digital
clipping. Edge wrapping, monster interactions and subjective/extended audio
remain unqualified. Evidence and hashes are in ignored
`build-xbox/hardware/doodle-hill-qualification.json`, `doodle-*.jpg`,
`doodle-gameplay-audio.json` and `doodle-final-return-status.json`.

## Hill Climb Racing and the Scratch font (October 26)

The same browser launched Hill Climb Racing. Right accelerated the jeep and
collected coins to Score=900; Left was exercised during that run. A crash
showed the authored loss message. A/Space restarted with Score=0, restored
fuel and Personal Best=900. Black/R and X/V do not bypass the game-over menu
because the project waits for Space there. During active play, X switched
to a motorcycle and different terrain; driving reached Score=420 with a
visible +200 airtime bonus. Black/R then reset that active run to zero and
restored fuel. Runtime pause, browser exit, a fresh jeep launch and another
exit succeeded. Both vehicle selection and these extra shortcuts are supplied
by the installed control profile; game scripts are unchanged.

A natural fuel expiry exposed a separate visual defect: “Out of Fuel!” lost
its ending, as did the countdown. The project uses the `Scratch` font, which
was missing and fell back to a wider serif font. The
[font compatibility repair](xbox-scratch-font.md) registers the open-font,
metric-compatible Scratch Savers replacement and retains its attribution.
Host rendering verified the original messages now fit their unchanged SVGs.
The existing Asteroids ending also rendered pixel-identically with/without
the newly registered font (SHA-256
`101f8d8a9c2983e7eb08f68556e9d5ef1532600108589cfd70ec116837e03e8e`
for both host regression PNGs, using a white background).

The font candidate `fe4329c2…` was uploaded to a separate folder and read
back. On hardware, `hill-font-countdown-20.jpg` shows the complete countdown,
`hill-font-fuel-watch.jpg` shows the complete fuel-loss message, and
`hill-font-crash-check.jpg` shows the complete crash message after Score=1,000.
A replayed each loss; after crash replay Personal Best=1,000 remained visible.
Runtime exit, a visually confirmed fresh browser launch and a second exit
passed again on this candidate. A fresh process/project load starts Personal
Best at zero; no cross-launch local score persistence is established.

The 7.995-second HDMI sample contained signal, peak 0.07767/RMS 0.01437,
without digital clipping. Extended terrain, precise brake/air control, fuel
pickup, authored Y pause and subjective/long-run audio remain outstanding.
Evidence is in `doodle-hill-qualification.json`, `scratch-font-deploy.json`,
`hill-font-*.jpg` and the saved hardware logs under ignored
`build-xbox/hardware`. The regular Games launcher still retains its prior
production build; this candidate has not been promoted across the library.

## Clean SVG-bounds regression

The clean build `3ba0cfe9d17ee50e49a829cd7ff60cd21e60c8b16ba76121c9e1111ec87404f3`
(9,687,040 bytes, menu tracing OFF) was installed separately in
`F:\scratch-bounds-clean-20261026` with complete FTP readback. Connect Four
launched after a 2.2-second A confirmation in keyboard mode, red first.
The controller sequence 1,2,2,3,3,4,3,4,4,6,4,7,4 produced the expected
red diagonal from column-one bottom to column-four row four, with “Red Wins!!!”
visible. Both D-pad red controls and right-stick yellow controls placed pieces
correctly. This is a pen-stamp, dynamic-list and SB2-rendering regression pass.
Evidence: `bounds-clean-connect-{launch,diagonal-win}.jpg`.

On that clean build, Connect Four’s held-A Green Flag reset to an empty board,
keyboard mode and red first; Exit returned to the browser. Asteroids then
launched normally, accepted thrust/turn/fire and reached Score 2,700 before
Game Over. Its complete ending text remained visible after the SVG bounds
change (`bounds-clean-asteroids-game-over-2700.jpg`). The eight-second HDMI
observation captured 7.990 seconds at 48kHz, peak 0.03418/RMS 0.00767, without
clipping in that sample; it establishes signal rather than subjective or
long-run audio quality.

Asteroids’ Green Flag replay restored the ship and three-life display; the
post-replay capture already shows Score 200 after gameplay resumed, so a
zero-score framebuffer is not claimed. Exit returned to the browser.
Evidence: `bounds-clean-asteroids-{replay,exit-browser}.jpg`.

## Breakout startup fault

The first hardware Breakout launch on `3ba0cfe9…` stopped at its background;
ten-second Start and the dashboard combo did not recover it. xemu reproduced
the failure. GDB recovered a null-sprite access in the Score monitor: the
archive’s stage is named Bühne, while the runtime normalizes it to Stage.
The [stage-monitor fix and fixture](xbox-sb2-stage-monitors.md) pass in the
actual Xbox executable under xemu, and original Breakout reached Score 20
with a visible paddle, ball and removed brick. Hardware retest is pending.

The [separate Breakout controller copy](xbox-breakout-controller.md) then
passed D-pad/right and left-stick/left steering, ±201 edge limits, Score 60,
loss, Green Flag replay with mode reset and renewed direct steering, and Exit
to the browser in xemu. Its original assets and non-input scripts are retained.
It has not yet been installed on the physical Xbox.
