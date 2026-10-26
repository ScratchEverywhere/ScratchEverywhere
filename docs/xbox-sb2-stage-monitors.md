# Scratch 2 stage names and monitor safety

Atari Breakout (LVRS) displayed only its background and stopped responding on
both Xbox hardware and xemu. GDB found an access violation at `0x000af500` in
`BlockExecutor::updateMonitors`, reading `sprite->isStage` at address `0x0c`.
The exception record was recovered from the Xbox kernel stack after its fatal
handler stopped returning to the application. Sampled relevant code bytes
matched the loaded ELF; a complete `.text` comparison also includes loader
patches and is not used as an exact runtime-image assertion.

The archive names its stage `Bühne` and targets its Score watcher at that name.
`parseSb2Object` normalizes stages to `Stage`, but the monitor importer only
recognized the English name. The watcher therefore could not find its owner.
The importer now maps the original stage name to the runtime stage target.
The shared monitor updater also skips missing targets before dereferencing
sprites or invoking monitor handlers. It does not substitute a different owner.

## Verification

`python3 examples/make-sb2-stage-monitor-test.py OUTPUT.sb2 [STAGE_NAME]`
creates a small fixture plus adjacent controller profile. Its default stage is
`Bühne`; it includes global/local variable and list monitors, a frame counter,
and a nonexistent target. The Xbox build in xemu showed Score 17 and LocalScore
99. Fresh A/B inputs changed these to 18/100 while Frames continued advancing.
GDB read Frames 625, global list `[red, blue]`, local list `[local]`, correctly
normalized global targets and retained local targets. The missing target did
not crash the game. This establishes list data/scope, not list-widget rendering;
the current SB2 importer still assigns list watchers the default variable mode,
so those two widget values appear blank. That separate limitation remains open.
The runtime pause menu and Exit also remained usable.

Candidate `743eaeaf12720757fe72622bcea164ee01c90c58553256f075417e8d1858b87f`
(9,687,040 bytes) then launched the original Breakout archive in xemu. Its logo
animation completed, the ball/paddle appeared, a brick disappeared and Score
reached 20. Evidence and matching ELF/ISO are retained in ignored
`build-xbox/breakout-controller/monitor-fix/`, including `fixture.png`,
`fixture-monitors.json`, `fixture-stage.json` and `breakout-playing.png`.
The emulator used a GDB-only network initialization bypass. Physical recovery
and testing of this candidate are pending; the older regular Games launcher
has not been replaced.

The subsequent [bitmap text-layer fix](xbox-sb2-bitmap-layers.md) restores the
life count, subtitle and Game Over text in xemu. Physical retesting and
controller usability remain separate qualification checks.
