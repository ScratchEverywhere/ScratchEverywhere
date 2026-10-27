# Tetris hardware and ending checks

The tested project is Tech-Coder's Tetris remake, original SB3 SHA-256
`df5ccfabe2de08f27b6a34574b268cf3050afb161e2a7ee1086901e076d14a1d`.
Its scripts and assets remain unchanged. The sidecar supplies arrow keys from
the D-pad/left stick and Space from A. Up rotates, Down soft-drops, and A
hard-drops. Hold Start for the runtime pause menu; Green Flag replays the game.
A short Start also replays a project that has ended with `stop all`.

## Ten-line controller run

On October 26 evening, ordinary Player Two controller input on the physical
128 MB Xbox completed 28 placements and ten single-line clears, using each
of the seven tetromino families four times. The HDMI board after every
placement matched the host's predicted locked cells. The helper
used rotations, horizontal movement and hard drop, inspecting video after
commands rather than assuming delivery. No debugger or game-state writes
were used for this hardware run.

The full runtime-pause snapshot at **2026-10-27 03:17:18.420916 UTC** confirms
`lines cleared=10`, `score=400` and `level=1`. The pre-pause image independently
shows those three HUD values. This run used diagnostic XBE `431b16c7…`;
it does not establish a normal-build repetition by itself.

A subsequent neutral-input video sample measured four one-row intervals at
Level 1: 0.55966, 0.59013, 0.52896 and 0.55087 seconds. Normal controller
Green Flag replay restored the Level/Lines/Score HUD to zero. Two Level 0
intervals measured 0.63346 and 0.64658 seconds. These small receive-timestamp
samples support faster gravity at Level 1; they are not precise display-frame
timings. After another replay, a 180 ms host Down command moved the same piece
five rows in approximately 0.54 seconds, with the same shape and an unchanged
empty fixed board. Neutral input had moved it one row in approximately
0.66 seconds. The durations describe host commands and observations.

A 30-second HDMI recording contains exactly 1,440,000 stereo frames at 48 kHz,
peak 0.101776 and RMS 0.017528, with no full-scale integer samples. Six
one-second probes at five-second intervals match the original music asset
`797aa2b40384302701a8ba13caf78f39.wav` with correlations 0.97595–0.99280.
Their source offsets are 2.00925, 7.00925, 12.00925, 17.00925, 22.00925 and
27.00925 seconds. This establishes signal and sampled timing agreement,
not a subjective listening pass.

Local evidence is under ignored `build-xbox/hardware/`:
`tetris-rotation-run1-results.json`, its controller receipt and placement
images, `tetris-rotation-run1-before-pause.jpg`,
`tetris-level{0,1}-fall.json`, `tetris-level1-replayed.jpg`,
`tetris-softdrop-current.json` and its before/after images, plus
`tetris-rotation-long-audio{.wav,.json,-segments.json}`. The corresponding
full pause snapshots are in `build-xbox/variable-watch/hardware-run6.jsonl`
and `hardware-run7.jsonl`.

## Focused authored score-cap test in xemu

The project checks `score > 99999999` in its `clear filled rows` procedure.
Reaching that cap through normal play is separate from verifying the handler.
The following are **debugger-seeded boundary probes, not a playthrough**.

An isolated xemu 0.8.136 guest used normal diagnostics-off XBE SHA-256
`2b8b57782fbff6363c413712f36cffdf37083fad48c77909bc866e02bb67eb6b`
and matching ELF symbols. Its HDD and EEPROM were private copies; the user's
running xemu session was not changed. The source SB3 and executable were not
patched. GDB supplied only the score, one complete bottom grid row, and the
current piece's X/Y. Ordinary gravity, locking, line removal, scoring,
comparison and broadcast execution then ran.

| Probe | Score after the line clear | Lines | Result |
| --- | ---: | ---: | --- |
| Seed score 99,999,959 and one complete row | 99,999,999 | 1 | Game continues; no ending broadcast |
| Seed score 99,999,960 and another complete row | 100,000,000 | 2 | `Winner!` observed at the broadcast entry; project stops |
| Invoke the ordinary Green Flag function | 0 | 0 | Level 0; a new piece falls; project runs again |

Both the authored `Winner!` and `Game Over` handlers show the same TETRIS title
card and execute `stop all`. There is no separate victory message in this
revision. The winning probe confirms the title sprite's ghost effect changes
to zero and the project stops. The final replay probe invokes
`greenFlagClicked()` through GDB; it is not a controller replay test from
victory. The ordinary hardware replay above exercises the controller path
after Level 1 separately.

Evidence is in ignored `build-xbox/xemu-tetris-cap/`:
`manifest.json`, `boundary-verification.json`, `boundary-below.json`,
`boundary-win-complete.json`, `boundary-replay.json`, the probe/observer
sources and `gdb.log`. The event observer samples the same paused entry twice;
its raw observations are not a count of distinct broadcasts. The final
boundary/replay assertions pass. The disposable guest and GDB are closed.

## Repetition on the normal dashboard build

Normal diagnostics-off XBE `2b8b5778…` was installed at the existing dashboard
path `F:/Games/ScratchEverywhere/default.xbe`. The previous XBE, SHA-256
`78d2fe178ae621a7d591d08af8439d829a49536686557f462eeb3df7d1788df5`,
is preserved on the Xbox as `default-78d2fe17-20261027.bak` and in the local
deployment evidence. The upload temporary file, promoted executable and backup
each passed a full byte-for-byte FTP readback. The twenty games plus Xbox-Test
remain installed; only the launcher XBE changed.

The new launcher booted to the populated browser and selected Tetris normally.
Fifteen controller placements, all matching the visible board, produced a
**three-line clear and Score 300**. Normal pause-menu Green Flag replay restored
the zero HUD. A separate attempt to build a four-line well completed 22 matched
placements, then stopped when the helper could no longer isolate a falling
piece. That attempt did not establish a four-line clear or a game crash.

Six further hard drops produced the authored TETRIS ending card. An eight-second
HDMI capture after that ending contains 384,000 stereo frames, all zero. A
500 ms host Start press replayed the game to the zero HUD. Right moved the new
piece one column; A placed it at the bottom. Another ordinary controller run
then completed **32 matched placements, ten lines, Score 400 and Level 1** on
this normal executable. Exit returned to the populated browser.

A separate 30-second normal-build audio capture has peak 0.101776 and RMS
0.017835, no full-scale samples, and six theme matches at correlations
0.99373–0.99804 with exact five-second source-offset progression. A later
120-second recording has 5,760,000 stereo frames and no full-scale samples.
Its twelve one-second probes from capture seconds 0 through 110 match the
theme at correlations 0.96091–0.99824. Two source-offset wraps give consistent
repeat periods of approximately 38.699 seconds; this does not establish
gapless transitions. Runtime pause was commanded at capture second 99.857.
The current clip finished at second 114.136, followed by 5.864 seconds of
digital silence while its script loop was paused. The probe at second 119 is
recorded as silent, with no fabricated correlation or source offset.

The 120-second recording's first stereo frame peaks at 0.533691; every later
frame remains at or below 0.101776. The origin of that isolated initial sample
is unresolved. These objective comparisons do not establish subjective sound
quality or clean capture startup.

A fresh browser launch also passed. After leaving this new game paused for
longer than the music clip, a separate eight-second recording contained only
zeros. Ordinary B resume restarted the theme: seven one-second probes match
source offsets 2.561875 through 8.561875 seconds, at correlations
0.97652–0.99370. The resumed eight-second capture has peak 0.101776 with no
full-scale samples. This distinguishes the paused loop waiting for execution
from music failing permanently after the clip ends.

This fresh project also returned cleanly to the populated browser. A second
dashboard-to-launcher start reached that browser by approximately 18.43 seconds
after the request. The retrieved runtime log chapter has no variable-watch,
menu-input or pause-state diagnostic markers; the live receivers recorded no
normal-build diagnostic packets.

Normal-build evidence is under ignored `build-xbox/hardware/`:
`normal-primary-tetris-well1*`, `normal-primary-tetris-four1*`,
`normal-primary-tetris-loss1*`, `normal-primary-tetris-replay-cells.json`,
`normal-primary-tetris-extended1*`,
`normal-primary-browser-after-tetris.jpg`,
`normal-primary-tetris-audio{.wav,.json,-segments.json}` and
`normal-primary-tetris-120s{.wav,.json,-match.json}`. The fresh-launch pause and
resume captures use `normal-primary-tetris-paused-eof-audio*` and
`normal-primary-tetris-resume-eof-audio*`. Deployment, backup,
game-list and condensed verification records are under
`build-xbox/primary-normal-20261027/`.

Four-line scoring through controller play and subjective listening remain
open. The focused score-cap test above is not a normal-play score-cap victory.
Earlier hardware checks and the other nineteen games remain in the
[twenty-game qualification matrix](xbox-game-qualification.md).
