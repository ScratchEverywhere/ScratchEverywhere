# Appel clone-budget failure

Appel's missing terrain was caused by an empty Boolean input reading stale
stack data. Its Levels sprite has an `if` with no CONDITION input around a
disabled level-setup script. `Scratch::getInputValueAs<bool>` returned success
without assigning the output when neither an input nor a field existed. The
caller declared an uninitialized `bool`, so the disabled setup could run.

That extra setup sent broadcasts to already-created clones. Background and
StatsLives clones then created further clones and exhausted the normal
300-clone limit before Foreground could allocate its terrain grid.

## GDB evidence

The test used the actual Xbox runtime in xemu. Because the available BIOS
reported only 64MB to the kernel despite xemu having 128MB configured, the
diagnostic ZIP omitted 45 audio payloads. Its `project.json` was byte-identical
to the full Appel archive, SHA-256
`5dd396278357332097aa16d2c0da88d8e650c47508adaa233f266d71830864ec`.
Networking was skipped with GDB for these offline script tests. These are
diagnostic experiments, not full-game or audio qualification.

In the unmodified runtime, a breakpoint immediately before the `if` branch
found the Levels block had only a SUBSTACK input, while its actual condition
stack byte was 1. Reading the optimized C++ local through DWARF was unreliable;
the test read the byte used by the generated compare instruction directly.
The clone trace reproduced 120 StatsLives and 172 Background clone attempts,
including creation by clones themselves, and reached the 300-clone limit.
The corresponding physical run had no Foreground clones.

A controlled run of the same binary changed only that Levels condition byte
to zero in GDB. The live population became:

| Sprite | Clones |
| --- | ---: |
| Foreground | 63 |
| Background | 63 |
| StatsLives | 10 |
| Wallpaper Tiles | 8 |
| menu | 12 |
| Total | 156 |

The compiled source fix then produced condition byte 0 and the same 156 live
clones without a debugger condition change. The clone limit stayed at 300.
Both value and typed input readers now assign defaults for absent inputs and
null reporter blocks: false, zero or an empty value as appropriate.

Ignored proof files in `build-xbox/xemu-debug` include
`appel-clones-before-fix.jsonl`, `appel-clones-controlled-false.jsonl`,
`appel-controlled-clone-population.json`, `appel-clones-compiled-fix.jsonl`
and `appel-compiled-fix-population.json`. The fixed diagnostic XBE and its
matching ELF symbols are retained there too.

## Related checks

GDB also caught an emulator startup abort while parsing an empty Settings.json.
The settings reader now rejects malformed/non-object JSON and uses defaults,
preserving the original file. This let the script diagnostic proceed. Embedded
builds also needed a guard around the menu-only pause pointer in the Xbox
Start/replay code; embedded projects compile without ENABLE_MENU.

`examples/make-empty-input-test.py` generates an integration regression with
empty `if`, `if/else` and addition sockets. Its expected final monitors are
PASSES=3, ERRORS=0 and EMPTY ADD=7. It is a test fixture, not one of the ten games.

Full-project hardware verification is recorded in `xbox-game-qualification.md`.
The complete project, including every audio payload, remains installed on the
physical Xbox; the diagnostic archive is never deployed over it.

## Full-project Xbox verification (October 25, 2026)

The complete Appel archive ran on the 128MB Xbox with the native allocator,
480p and the expanded-RAM XBE flag. The tested XBE SHA-256 is
`01565e87aa3b86b0cb41ed2aac82e8140552b97b564f253f26b050c986996423`,
installed at `F:\scratch-gpu-range-20261025\default.xbe`.

An initial source-fixed build produced the expected 156 menu clones, including
63 Foreground tiles, but displayed only clear colors. The SDL XGU driver could
allocate textures and its vertex arena above 64MB, outside pbkit's default DMA
context range (`MAXRAM`, 0x03FFAFFF). Restricting those three GPU allocations to
MAXRAM restored the browser, title, level picker and terrain. CPU allocations
can still use expanded RAM. The dependency patch applies cleanly to its pinned
upstream source. Existing Catalog checkouts cache applied patches: refresh SDL3
when adopting this change (`-DCL_REFRESH_SDL3=ON` for that configure, then remove
the refresh setting for subsequent builds with `-UCL_REFRESH_SDL3`).

On the physical console, the level labeled LEVEL 1 entered play (its internal
LEVEL # is 2). Right movement, A+jump, raised platforms, scrolling ground and
spikes rendered correctly. Four apples were collected. A pause snapshot then
reported APPLES=4, GAME STATE=PLAY, 168 live clones, 63 Foreground and 63
Background clones. The game expanded MAP and MAP R to 16,384 entries during
play. Exit returned cleanly to the browser.

The generated empty-input fixture also ran on hardware and displayed/logged
PASSES=3, ERRORS=0, EMPTY ADD=7. It was removed from the console afterward.
Proof is retained in ignored `build-xbox/hardware/appel-fixed-gpu-range.log`,
`appel-terrain-four-apples.jpg`, `appel-fixed-exit-browser.jpg` and
`empty-input-result.jpg`. At that point, full-level completion, death/replay and audio remained
separate qualification work; subsequent checks are in the qualification log.

A later extended-test boot still produced a purple browser and black Appel
scene despite the GPU range restriction. Its pause log retained the correct
156 menu clones and all 63 Foreground tiles. The range restriction is a valid
DMA constraint and improved the tested run, but it does not establish that the
intermittent startup rendering failure is fully resolved. Retrying the same
XBE returned a visible browser. Evidence: `appel-profile-black-startup.log`.
