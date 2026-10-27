# Frogger hardware checks

The original cs50 Frogger SB2 archive has SHA-256
`6df88eff3a23796d3abc6ebde5cf7ec9b1df611ff6937c093a6cc59eb048a662`. Its sidecar
maps arrows to both the D-pad and left stick. The initial checks below used the
original scripts and assets. A separately prepared ending/replay repair is
described afterward.

On October 26, synchronized build `be4bc0b4…` was uploaded separately to
`F:/scratch-sync-20261026/default.xbe`, with full FTP readback verification.
Normal browser selection launched the game. D-pad Left, Right and Up moved the
frog; traffic collisions displayed the complete Game Over message and the
authored timer bubble. Runtime pause and Green Flag repeatedly restarted the
game. An eight-second HDMI capture contains non-clipping audio (peak 0.328125,
RMS 0.013817); this establishes signal, not subjective or long-run sound
quality.

One continuous Up crossing cleared all five traffic lanes and reached the bank
at captured screen position (360, 252). Input/video delay carried that attempt
into the river, where the frog died. Another crossing reached the river's edge,
and the frog was carried left while no direction was held: captured X moved from
360 toward the left edge at approximately the same Y. The video-guided helper
then lost tracking and released its inputs. That is not evidence of a game
crash. A later traffic collision was also observed.

The temporary helpers only send controller reports and inspect live video; they
do not modify the game or runtime. These attempts do not establish lily-pad
scoring, a full river crossing or the seven-pad win condition. Those checks
remain open. Earlier builds established stick movement, browser exit and repeat
launch; they are recorded in the qualification matrix.

Local evidence is under ignored `build-xbox/hardware/`: the installed archive
and profile copies, `upstream-sliders-deploy.json`, `frog-sync-start.jpg`,
`frog-sync-up-first.jpg`, `frog-sync-road-plan.jpg`,
`frogger-sync-audio.{wav,json}`, `frog-corridor-road-crossed.jpg`,
`frog-corridor3-road-crossed.jpg`, `frog-corridor3-08.jpg` and the corresponding
`frog-corridor*.json` controller/position traces. Video freshness was checked at
15 ms frame age with one viewer during the later attempts.

## Seven-pad ending and replay repair

The original stage waits for the 128.37-second music track to finish before
checking score and broadcasting `win`. The seventh pad leaves the frog in place;
after the pad's one-second wait, its occupied-pad check can kill that frog even
though the score is already seven. A second startup-order problem affects
replay: sprite loops can read the old score/alive state and stop before the
stage's Green Flag initializer resets those shared variables.

Prepare a separate copy of the tested original archive:

```sh
python3 scripts/prepare-frogger.py ORIGINAL.sb2 PREPARED.sb2
```

The preparation moves the win broadcast to an independent score waiter, stops
pad collision loops after seven points, and starts the 56 original sprite
initializers through a stage message after the shared-state reset. The win
waiter yields once so it cannot act on an old score before that reset. Movement,
one-point scoring, and occupied-pad death before seven points retain their
original blocks. All 32 assets and credits are byte-identical. Reversing the
isolated script edits yields exactly the original JSON. The script rejects a
different source revision and refuses to overwrite any existing output.

The prepared archive is 1,409,001 bytes with SHA-256
`6c747a924398677126215e8468d6b33a5c275c74b02cfe05aba1fd4dd05bd23f`.
Retain the original arrow-game sidecar alongside the deployed filename.

### xemu/GDB verification

These are debugger-positioned collision probes, **not a normal playthrough**.
Only frog X/Y were placed at each actual pad's position. The engine executed
collision detection, scoring, broadcasts, costume changes, respawn and ending
scripts without direct score/alive/costume writes.

The original reached seven points, but after 90 further input polls it had
alive 0 and no victory display. The final prepared copy scored once at each of
the seven pads, respawned after the first six, displayed victory within six
input polls of the seventh, and retained alive 1 and victory after 90 more
polls. Normal pause-menu Green Flag replay reset the score, pads and win display;
subsequent pad scoring worked. Revisiting that occupied pad then caused death
at score 1. A second menu replay after death also allowed scoring again.

The final copy was checked against the established trace-off engine build
`57a229b0…` with matching symbols. Evidence is under ignored
`build-xbox/xemu-frogger-score/`: `preparation-v2-validation.json`,
`seven-pad-probes-v2.json`, `v2-seven-pads-after-90-polls.json`,
`v2-menu-replay-result.json`, `v2-first-pad-after-replay.json`,
`v2-occupied-pad-death.json`, `v2-menu-replay-after-death.json`,
`v2-scoring-after-death-replay.json` and `gdb-v2.log`.

### Installation on the physical Xbox

The prepared copy replaced `E:/ScratchEverywhere/Frogger-cs50.sb2` with full FTP
readback verification. The original is preserved as
`Frogger-Original-20261026.sb2.fixture`, outside the browser's game extensions.
The unchanged control profile has SHA-256
`46c9f91e50285b71b1bd6aa010b0f5fb2b6ff6605071d60c23736f6be1e1ed39`.
The installation retained twenty games plus Xbox-Test and the established
`be4bc0b4…` runtime. This project-only repair does not require a new XBE.

On that runtime, normal browser selection launched the prepared project. D-pad
Left and left-stick Right moved the frog on the starting strip; Up caused a
traffic collision and the complete Game Over/timer display. Pause-menu Green
Flag restored the starting position and removed the death display; a new Right
press moved the frog again. Exit returned to the populated game list after A
release. A fresh browser launch, another Left movement, and a second exit also
passed. All actions used the Player One KVM route and one continuous viewer.

Hardware evidence is under ignored `build-xbox/hardware/`:
`frogger-win-deploy.json`, `frogger-win-v2-{selected,launched,left,stick-right}.jpg`,
`frogger-win-v2-{road-death,pause,replay,replay-movement}.jpg`,
`frogger-win-v2-browser-after-exit.jpg`,
`frogger-win-v2-{relaunch-selected,fresh-relaunch,fresh-movement,second-browser}.jpg`.

At that stage, full river crossings, seven-pad completion through controller
play, and the physical console's victory path remained unverified. The emulator
probes alone do not establish those requirements.

## Controller-only river scoring — October 26 evening

The same prepared archive subsequently reached six points in one live round on
the physical Xbox. Diagnostic XBE `431b16c7…` exposed the original frog and moving
platform positions through the optional read-only sprite watch. Host helpers
sent ordinary Player Two D-pad reports and inspected those observations and
HDMI video. No debugger, score, position, costume or map writes were used.

Each scoring check below is a full runtime-pause snapshot, rather than an
inference from a delayed video frame. All six snapshots have `alive=1`.
Times are UTC on October 27 (October 26 local time).

| Snapshot time | Score | Newly occupied pad X |
| --- | ---: | ---: |
| 02:04:20.037535 | 1 | 2 |
| 02:08:05.921045 | 2 | 71 |
| 02:11:43.821941 | 3 | -203 |
| 02:19:08.045844 | 4 | 140 |
| 02:24:18.787020 | 5 | -137 |
| 02:28:12.556994 | 6 | -67 |

The sixth crossing used HDMI tracking after the bounded watcher reached its
4,096-sample limit. The game continued normally; the limit did not restart the
project. `frog-video-pad6-after-river.jpg` shows six occupied pads and the frog
back on the starting strip. The next road attempt ended in a traffic death at
score six. A normal pause-menu Green Flag replay then scored the far-right
pad at X=208, followed by X=-203 and X=-137, before another river death.
Thus every pad has scored through controller play across these two rounds,
but seven points and the victory display in a single round remain unverified.

The temporary host route model initially used the literal offscreen reset
coordinates from the project. Actual sprite fencing places some resets farther
inside the stage; the host forecast was adjusted from observed positions.
No-window timeouts released input and preserved the round on the bank. Those
timeouts and the two deaths are retained as failed attempts, not game crashes.

A 30-second HDMI recording contains 1,440,000 stereo frames at 48 kHz. Six
one-second probes at five-second intervals match the original music asset
`f6ae0b03d27e33cfc1ed1fed4274661a.wav` with correlations 0.97658–0.99380
and source offsets advancing by exactly five seconds. Both channels of the
first captured frame are -32768; no later frame reaches either integer limit.
The origin of that initial full-scale frame is unresolved, so this recording
does not establish clean capture startup or subjective audio acceptance.

Local evidence is under ignored `build-xbox/`: sprite-watch
`frogger-six-pad-hardware-verification.json` and
`frogger-round2-verification.json`; variable-watch `hardware-run4.jsonl` and
`hardware-run5.jsonl`; hardware `frog-watch-staged2`, `frog-watch-pad2`,
`frog-watch-pad3`, `frog-watch-pad4d`, `frog-watch-pad5`, `frog-video-pad6`, and
`frog-round2-pad*` observations/controller receipts/captures, plus
`frogger-river-long-audio{.wav,.json,-match.json}`. The diagnostic build does
not establish a complete normal-build playthrough. Seven-pad victory, replay
from victory, and subjective sound checks remain open.
