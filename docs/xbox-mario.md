# Mario death and restart on Xbox

Will_Wam's Super Mario Bros. first-level remake launches from the regular Xbox
browser after the [calendar reporter fix](xbox-calendar.md). With normal
launcher `2b8b5778…`, ordinary Up/Right controller play crossed the first three
pipes and visibly stomped a Goomba. A later return to the beginning was
followed by an invisible player while the camera continued moving. These are not
accepted as clean loss/recovery passes.

The original archive is SHA-256
`93a957be2bcffda5e33df1c9e3af36ccfcad6ee37986955ab9df39378f08013d`.
Its controls are arrows for walking/jumping and Space for sprint/fire:
D-pad/stick arrows and A for Space in the installed profile.

## Diagnosis

An isolated xemu guest used the same normal XBE and its matching ELF. GDB
started the original Mario `Get Hit!` hat, then supplied input frames. No
position, score, collision, map or checkpoint state was written. This is a
focused death/recovery probe, not a level playthrough. The debugger slows frame
delivery; it does not establish a particular physical frame rate. The guest
also used the existing GDB-only networking-initialization bypass.

The audio revision additionally records both death-cue entries: each sees the
Stage sound-cleanup completion flag set before the cue starts. Both death/
recovery cycles and the movement/landing checks pass on that revision.

The project starts a frame-based death animation and a timed sound concurrently.
It then broadcasts `Restart Level` after the sound and a delay. In the original
probe, that broadcast occurred while Mario was still visible in death costume,
at Y=-88.4 with upward velocity 10.4. `Official Start` followed while the old
animation was still moving Mario. After initialization the old death loop
continued changing velocity/position instead of leaving a stable new round.

Two additional ordering problems appear when waiting for that animation alone:
fixed 0.2-second resets re-enable hits before level upload finishes, and
`Official Start` unfreezes physics before restoring Mario's position. Physics
can therefore queue `Get Hit!` at the old death position. Even keeping the hit
guard set until the end is insufficient if that queued handler runs after the
guard is cleared.

## Prepared copy

`python3 scripts/prepare-mario.py ORIGINAL.sb3 NEW.sb3` accepts only the tested
original revision and refuses an existing output or replacement of the source.
It creates a separate project that:

- Signals completion of Stage sound cleanup before playing the death cue,
  replacing the original 0.04-second assumption.
- Marks death-animation completion only after the original final Hide block.
- Waits for that completion as well as the original sound/delay before restart.
- Removes timed post-restart hit resets and enables hits after initialization.
- Keeps physics frozen until the initializer has restored position, visibility
  and velocities.

Movement, collision rules, difficulty, sounds, art and credits are preserved.
All 133 asset files, sprites other than Stage/Mario, and the other project
metadata are byte-identical. Next/parent/input block references were checked. The control
profile is unchanged. The prepared archive SHA-256 is
`7efb9b52ea4d987f63eb8958adae38f60784eb56ed79518998d2c39496791eba`.

The executable itself is unchanged; this is a repair to a separate game copy.
The normal-build xemu test passed two separately triggered deaths, one restart
per trigger, no spurious `Get Hit!` during initialization, and walking/jumping
after the second recovery. Both restarts returned a visible Mario at Y=-110
with zero velocity and both guards released. After jumping, collision resolution
landed at Y=-110.800003; touching-ground was true, vertical velocity was zero
and the visible position remained unchanged across another thirty input frames.
An earlier verifier expected the initial Y too precisely; the corrected check
uses grounded state and stability as well as the expected floor range.

The preparation tool reproduced byte-identical output and refused an existing
output, source-as-output, and an unrecognized input revision without modifying
the source. The preceding gameplay candidate also passed two ordinary hardware
deaths, automatic visible recovery, renewed walking/jumping/landing and a clean
browser exit. Its two recordings lacked the death cue, which motivated the
Stage-cleanup completion signal above. The continuous capture shows actual
Goomba contact, the full death pose/arc, and return to a stable opening area.
On the normal Xbox launcher, the audio revision has now passed an ordinary
Goomba death, automatic visible recovery, renewed walking/jumping/landing and
clean browser exit. Its 120-second HDMI recording has 5,760,000 stereo frames,
peak 0.12106, RMS 0.01642 and no full-scale samples. The complete original
2.71225-second death cue matches at capture time 42.88925s, correlation 0.96033.
Music resumes from its beginning; probes at 50–80 and 100–110 seconds match at
0.99403–0.99620 with continuous source offsets. The 90-second probe has lower
correlation 0.79111 and is not counted as an isolated music-quality pass. A fresh browser launch repeated all four opening pipes, then deliberately
walked into the authored first pit. Mario completed his death arc, automatically
returned visibly to the opening area, walked/jumped/landed again and exited
cleanly to the browser a second time. No debugger or state writes were used.
The repeat loss recording contains 2,880,000 stereo frames over sixty seconds,
peak 0.11417, RMS 0.01482 and no full-scale samples. Its complete death cue
matches at 26.876875s, correlation 0.99994. Music probes at forty/fifty seconds
resume with ten seconds of source-offset progression. A preceding ninety-second
music/gameplay recording has no full-scale samples and continuous sampled
source offsets through the four-pipe route.

The normal launcher remains installed, SHA-256 `2b8b5778…`, verified again
by full FTP readback before the final archive deployment. The original game is
preserved on the Xbox as `E:/ScratchEverywhere/mario-original-93a957be.fixture`;
the prepared archive keeps the usual Mario `.sb3` name and unchanged sidecar
profile. Temporary upload, final archive and backups passed full readback.
The backups use `.fixture` extensions so they add no selectable projects.

First-level completion and subjective listening remain open.

## Audio evidence from the original hardware run

Two 90-second, 48kHz stereo HDMI captures each contain 4,320,000 frames without
full-scale samples. Their peak/RMS values are 0.09384/0.01644 and
0.12558/0.01551. Nine one-second probes in the first capture match the original
music at correlations 0.99939–0.99968, with exactly ten seconds of source-offset
progression between probes. The second contains the original 2.71225-second
death cue at capture time 74.36325s, correlation 0.96033, and music resuming
from its beginning. These signal checks do not establish subjective listening
acceptance or explain every return to the beginning.

Local evidence is under ignored `build-xbox/xemu-mario-pipes/` (original and
candidate broadcast traces, matching artifacts, preparation/readiness probes)
and `build-xbox/hardware/` (`mario-normal-*` action receipts, frame histories,
audio and FTP deployment receipts). Earlier candidates remain recorded as
failed/incomplete checks, not accepted repairs.
