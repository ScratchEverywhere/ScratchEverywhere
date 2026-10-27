# Original Xbox handoff for Mike

October 27, 2026 · [Zachary's NXDK fork](https://github.com/ZacharyLeahan/scratcheverywhere-nxdk)

## A short version to share

Mike, we've been testing Scratch Everywhere on a real 128MB Original Xbox and
have twenty games installed. There is useful gameplay evidence for all twenty,
but qualification is still in progress. Highlights include six scored pipes
in Ultra Bird, a full Breakout board, ten lines in Tetris, a 2048 victory, and
several completed Sokoban puzzles. Appel's missing terrain was traced to a
shared runtime bug; its first level has been completed on a diagnostic build,
with a full repeat on the normal build still outstanding.

When I said adding games was tricky, I meant the installation workflow. The
installed-project browser lets us select and launch games. What we don't have
on Xbox is an in-app way to find, download or import another Scratch project.
We currently leave the app, return to XBMC, FTP the project and sometimes a
controller profile into the project folder, then relaunch. That's cumbersome
when trying games with my daughter. An import/receiver flow and clearer
controller setup would make that much easier.

Some changes are shared engine fixes, some are Xbox backend/input fixes, and
some are repairs to separate copies of individual games. Those are broken out
below so you can review the parts useful to upstream independently.

## What is actually installed and tested

The test console has **128MB RAM**. These results do **not** establish retail
64MB compatibility. The regular dashboard entry launches the normal NXDK
executable, with gameplay/menu/renderer tracing disabled, into the populated
project browser. Its tested XBE hash begins `b75e9beb`; the full deployment
record is in the [qualification log](xbox-game-qualification.md).

The build uses forced 480p, expanded-RAM permission and the native allocator
(`SE_USE_LWMEM=OFF`). Appel loading with lwmem remains a separate investigation.
Build instructions are in [the fork build notes](xbox-fork-build.md).

At the time of this handoff, fetched upstream `main` through
[`0bacdb6d`](https://github.com/ScratchEverywhere/ScratchEverywhere/commit/0bacdb6d0d7d7285d4ca8f46ae59e5cd6fb3a174)
is already included in this fork. All work is in Zachary's fork; this handoff
does not submit anything upstream. Hardware testing is paused at Zachary's
request.

**How to read the table:** results are concrete milestones, not blanket passes.
“Prepared copy” means a reproducible tool changed the game's project data or
scripts; it is not evidence that the untouched download behaves identically.
“Profile” means an adjacent controller-mapping JSON file, without changing game
scripts. Original project links identify the source, not a guarantee that its
latest online revision matches our tested archive. Detailed notes retain hashes
and distinguish older builds from current regressions.

| Game / original source | Demonstrated Xbox gameplay | Copy / remaining checks |
| --- | --- | --- |
| [Hopper Heros — pepper-prepper](https://scratch.mit.edu/projects/1184969400/) | Everywhere variant: movement/jump, losses at 3 and 8, Start replay, browser return and fresh launch. [Notes](xbox-hopper.md) | Original Everywhere archive + profile. Link is the original game credited in the README; the tested Everywhere variant came from the Discord examples. Longer progression and audio acceptance remain. |
| [Ultra Bird — Discord example](https://discord.com/channels/428359196719972353/428399682239332354) | Current normal build: six pipes scored, natural loss, Start replay to zero, fresh launch/flap and two clean exits. [Notes](xbox-ultra-bird.md) | Unchanged archive. No authored sound playback. A standalone Scratch project URL is not recorded; the link is the example channel. Shared KVM delivery remains open. |
| [Catch Apples — superd_1](https://scratch.mit.edu/projects/124652800/) | Score 22 at timer expiry; replay resets to 0/30; steering, fresh launch and exits. [Notes](xbox-catch-apples.md) | Unchanged game + profile. Bonus/rotten-apple checks, audio acceptance and intermittent browser handoff remain. |
| [Maze Starter — Scratchteam](https://scratch.mit.edu/projects/10128431/) | Walls block movement; complete route wins; clean Green Flag replay and renewed movement, including current normal build. [Notes](xbox-maze-replay.md) | Prepared replay-order repair + profile. No authored audio. Shared intermittent browser handoff remains. |
| [Snake — NemoNaturally](https://scratch.mit.edu/projects/511466864/) | All four D-pad directions, fruit/Score 125, loss, replay and fresh launch. [Notes](xbox-snake.md) | Unchanged game + profile. Explicit self-collision, clock/blade pickups and audio acceptance remain. |
| [Appel v1.4 — griffpatch](https://scratch.mit.edu/projects/60917032/) | Terrain restored. Diagnostic controller play completed Level 1 with 18 apples and Try Again. Current normal build repeats seven apples/checkpoint two and checkpoint respawn. [Notes](appel-clone-debugging.md), [route evidence](xbox-game-qualification.md) | Unchanged game + profile. Full normal-build level completion, lwmem loading and audio continuity/acceptance remain. Diagnostic completion used read-only live feedback; it is a different evidence scope. |
| [Tetris — Tech-Coder](https://scratch.mit.edu/projects/469540467/) | Current normal build: ten lines/420/Level 1 in 27 matched placements, including a double. Earlier normal run also scored a triple; top-out/replay and fresh launch checked. [Notes](xbox-tetris.md) | Unchanged game + profile. Controller-play four-line clear and audio acceptance remain. The extreme score-cap ending was tested with diagnostic seeding, not ordinary play. |
| [Frogger — cs50](https://scratch.mit.edu/projects/12221773/) | Six pads in one round; all seven positions scored across rounds; death, replay and fresh launches/exits. [Notes](xbox-frogger.md) | Prepared win/replay-order repair + profile. Seven-pad win in one hardware round and victory replay remain; audio-start anomaly unresolved. |
| [Atari Breakout — LVRS](https://scratch.mit.edu/projects/11007566/) | Final normal prepared copy: full board at 1,940/one life, sustained clear-board play, Green Flag reset, fresh launches/steering/exits. [Notes](xbox-breakout-controller.md) | Prepared controller/eraser/paddle/serve repairs. No authored audio playback. Intermittent KVM delivery and a faint scaled boundary line remain. |
| [PacMan — cdaniels7088](https://scratch.mit.edu/projects/13701368/) | Current normal build: walls/turns/pellet scoring, natural Game Over, Start replay and renewed scoring, fresh launch and exit. [Notes](xbox-pacman.md) | Original game + profile; engine fixes restore collision and ending costumes. Power-pellet/ghost interactions and board completion remain. |
| [Space Invaders — RokCoder](https://scratch.mit.edu/projects/253610111/) | Start/fire, steering, 450 points, shield damage/life loss, replay, fresh launch and exit. | Original scripts + profile. Full wave and audio acceptance remain. |
| [Asteroids — Will_Wam](https://scratch.mit.edu/projects/32219470/) | Thrust/turn/fire, 9,200 points, Game Over, Green Flag replay and fresh launch/exit. | Original scripts + profile; shared SB2 text repair. Further progression and audio acceptance remain. |
| [Sokoban — Gammastr20](https://scratch.mit.edu/projects/24490009/) | Puzzles 1–3 completed. Repaired puzzle ten completed on hardware in 698 moves/171 pushes, advancing naturally to eleven; restart and fresh launches checked. [Notes](xbox-sokoban.md) | Prepared three-cell puzzle-ten repair + profile. Later puzzles/full playthrough and audio acceptance remain. |
| [2048 — JereTheJuggler](https://scratch.mit.edu/projects/19554088/) | Normal unseeded victory at 20,264; A continues beyond victory; loss/replays and fresh launch/exits. [Notes](xbox-2048.md) | Prepared A acknowledgement + profile. No authored audio playback. Shared KVM/browser handoff remains. |
| [Retro Racing v0.8 — griffpatch](https://scratch.mit.edu/projects/26999060/) | Acceleration to 108 MPH, braking/steering/off-road slowdown, Y pause, replay and fresh launch/exit. [Notes](xbox-retro-racing.md) | Original scripts + profile. Full lap, traffic/drift and audio acceptance remain. |
| [Hill Climb Racing v1.0 — griffpatch](https://scratch.mit.edu/projects/56667360/) | Jeep score 1,000, motorcycle 420/+200 airtime, crash/fuel loss, A replay and vehicle swap. | Original scripts + profile; shared font/text repairs. Extended terrain, fuel pickup, precise controls and audio acceptance remain. |
| [Super Mario Bros. — Will_Wam](https://scratch.mit.edu/projects/196684240/) | Four opening pipes, ordinary enemy/pit deaths, automatic visible recovery and renewed movement, two launches/exits; source-matched death cues. [Notes](xbox-mario.md) | Prepared death/audio/restart ordering repair + profile. Full level and subjective audio acceptance remain. |
| [Doodle Jump — xamuil2](https://scratch.mit.edu/projects/399123680/) | Steering/platforms, at least 1,044 m, fall/automatic replay, fresh launch and exit. | Original scripts + profile. Edge wrapping, monsters, extended play and audio acceptance remain. |
| [Jumper — Depicklator](https://scratch.mit.edu/projects/103069687/) | Levels 1–4 cleared, level 5 rendered, lava death/replay, Black restart and Y+Black full restart. | Prepared A/menu-readiness repair + profile; shared SVG-bounds fix. Later levels, audio acceptance and intermittent browser handoff remain. |
| [Connect Four — aaronzoll](https://scratch.mit.edu/projects/52769452/) | Both colors/control sets, stacking/full-column rejection, vertical/horizontal/diagonal wins, replay and repeated launch. [Notes](xbox-sb2-dynamic-lists.md) | Prepared initializer/monitor repair + profile. One initial mouse-mode launch unexplained. No authored audio playback. |

The [full qualification matrix](xbox-game-qualification.md) is the source of
truth for unfinished checks. Ten original download records are tracked in
[`xbox-additional-games.json`](xbox-additional-games.json); the other download
manifests and raw images/logs/audio remain local under ignored `build-xbox/`.
This handoff is not a hosted evidence bundle or redistribution of the games.

## Break the changes into independently reviewable groups

Commit titles sometimes say `xbox` because that is where the failure was found.
The implementation's scope matters more than that prefix. Passing Xbox tests
does not establish a regression pass on every other platform.

| Group | Concrete change / relevance | Scope and review entry point |
| --- | --- | --- |
| Empty Scratch inputs | Assign false/zero/empty defaults rather than reading stale data. Appel's disabled setup otherwise ran and recursively created clones, starving terrain of its normal clone budget. The 300-clone limit was retained. | **Shared runtime.** [Diagnosis and compiled-fix evidence](appel-clone-debugging.md); current implementation in [`runtime_inputs.cpp`](../source/runtime/runtime_inputs.cpp). Separate this from the GPU dependency patch in the same original commit. |
| Scratch 2 loading and rendering compatibility | Preserve computed list selectors in both engines; index named costumes/backdrops; resolve localized stage monitor owners safely; compose bitmap text layers and preserve SVG artwork/text bounds. These affect Connect Four, PacMan, Breakout, Asteroids and Jumper. | **Shared parser/runtime/image handling.** [Dynamic lists](xbox-sb2-dynamic-lists.md), [monitor safety](xbox-sb2-stage-monitors.md), [bitmap layers](xbox-sb2-bitmap-layers.md), [SVG bounds](xbox-sb2-svg-bounds.md). Scratch font registration is also shared: [font notes](xbox-scratch-font.md). |
| Streaming audio | Preserve fractional resampler phase and decoder lookahead across callbacks instead of discarding samples. | **Shared mixer**, affecting platforms that use it. [Fix and deterministic host/xemu/hardware checks](xbox-audio-resampling.md); [commit](https://github.com/ZacharyLeahan/scratcheverywhere-nxdk/commit/72fd448e82dca3c4c44f323d0205c0c213b15824). This does not settle every game's audio acceptance. |
| Color sensing | Restore CPU asset colors on demand on Xbox; include pen pixels in color queries; sample exact bitmap texels. Retain partial collision-mask edge cells and cull distant candidates. | **Mixed.** Image/pen readback and exact sampling have Xbox implementations/guards; collision-mask edge retention and candidate handling are shared. [Scope and regressions](xbox-color-sensing.md), [latest PR #22](https://github.com/ZacharyLeahan/scratcheverywhere-nxdk/pull/22). The identical final 38-case fixture passed in xemu and on hardware. |
| Startup, GPU and memory | Deterministic screen-space vertex setup; keep GPU buffers within the DMA-addressable range even on expanded-RAM hardware; permit expanded CPU RAM and retain complete map data. | **Xbox renderer/build/SDL dependency work.** [Startup notes](xbox-startup-rendering.md), [Appel GPU diagnosis](appel-clone-debugging.md), [build options](xbox-fork-build.md). Historical blank-screen failures and separate allocator limits are documented rather than assumed universally solved. |
| Browser, controllers and platform services | Open the installed-project browser at boot/exit; support active-controller changes, SB2 profiles and multiple bindings; isolate menu confirmation from game input; use native calendar APIs; add replay/pause/reset controls. | **Xbox-specific behavior**, including guarded changes in shared menu/runtime files. [Menu/input notes](xbox-menu-input.md), [calendar](xbox-calendar.md), [hardware history](xbox-hardware-testing.md). Some cross-controller handoff failures remain. |
| Profiles | Map D-pad/sticks/buttons to the game's existing keyboard controls in a neighboring `<filename>.json`. | **Per-project configuration**, not game-script changes. [Examples and naming](../examples/README.md), [Appel profile](../examples/appel-xbox-controls.json). Our tested profiles were installed alongside the archive. |
| Prepared game copies | Maze replay yield; Frogger win/replay ordering; 2048 A acknowledgement; Sokoban puzzle-ten map repair; Mario death/audio ordering; Breakout controller/eraser/paddle/serve repairs; Jumper menu readiness; Connect Four initialization/monitor repair. | **Project changes**, not engine fixes. Reproducible `scripts/prepare-*.py` tools create separate copies; linked game notes record their effects/hashes. Review these separately from runtime compatibility patches. |
| Diagnostics and test tools | Isolated xemu/GDB sessions, optional UDP log/variable observations and small generated regression projects. | **Developer tooling / optional Xbox diagnostics.** [GDB setup](xbox-guest-debugging.md), [network logging](xbox-network-logging.md). Normal installed gameplay tracing is disabled; generated fixtures do not count as games. |

For an upstream discussion, a useful order is shared semantic fixes first,
Xbox backend/input changes second, and optional tools/project adaptations last.
Small PRs should carry the reproducer and exact test scope for that change.
The fork has already incorporated upstream's overlapping ROMFS/archive
lifetime repair; its older workaround should not be proposed independently.
See [the synchronization notes](xbox-fork-build.md).

## What “adding games is hard” means

The working browser is a **selector for projects already on disk**. There is
currently no Xbox in-app project search/download/import flow. “No auto selector”
was shorthand for that missing acquisition flow, not a claim that installed
games cannot be selected.

The code already has a **Controls editor under per-project settings**:
[`projectSettings.cpp`](../source/menus/projectSettings.cpp) opens
[`controlsMenu.cpp`](../source/menus/controlsMenu.cpp), which discovers project
keys and saves their mappings. That route has not been hardware-qualified in
this handoff. Our tested mappings were supplied as sidecars so they could be
reproduced and checked. The request is to improve discoverability and setup,
not to claim there is no existing mapping UI.

Our current workflow is:

1. Find a Scratch project and obtain its `.sb3` or `.sb2` archive on a computer.
2. Determine its keyboard/mouse controls and, if needed, prepare a controller
   sidecar. Some mouse-only interactions need a prepared game copy instead.
3. Leave Scratch Everywhere and return to XBMC, which provides FTP on this
   test setup.
4. FTP the archive to `E:\ScratchEverywhere\` and put its profile beside it,
   using the exact filename plus `.json`, for example `Snake-NemoNaturally.sb3.json`.
5. Relaunch Scratch Everywhere, select the installed game, and check it works.

That loop is particularly awkward when one person is managing files and
controllers while a child is waiting to play. A downloadable game is also not
automatically compatible: engine behavior, project requirements and controls
still need testing. Improving installation would remove that first barrier
without implying that every Scratch project will run.

Reasonable next product ideas, **not implemented features**, are:

- An Xbox-side receiver/import screen that can accept an archive from a phone
  or computer while the app stays open, report the save location and refresh
  the project list.
- Improve the existing Controls editor with arrows/WASD/action presets, a
  visible mapping summary and a test screen before entering the game; verify
  its Xbox navigation and preservation of multi-button mappings.
- Clear errors for unsupported/corrupt projects and memory pressure, plus a
  route back to the project browser after a failed load.
- Later, an optional project-ID/URL download flow if networking, storage and
  service requirements make it practical.

## Remaining issues worth discussing with Mike

The strongest immediate engine evidence is available in the small regression
fixtures. For end-user polish, installation/profile setup and reliable menu
handoff are more valuable than increasing the installed-game count alone.

The [KVM input investigation](xbox-kvm-input-delivery.md) has caught neutral/A
edges during a held remote confirmation and some missed remote movement. It
has not isolated the failing transport layer. That evidence should not be
generalized into a claim that ordinary physical controllers fail in the same
way. Some exits unexpectedly reloaded the first browser game, 2048; repeated
successful exits do not close that intermittent bug.

Appel's latest normal-build run reached checkpoint two with seven apples and
demonstrated a return to that checkpoint after a fall. Testing paused before
crossing the moving lifts. A 120-second HDMI capture confirms music, but source
offsets do not consistently track elapsed time; one probe is weak, and the
initial capture-sample anomaly remains unexplained. It is not an audio
continuity or subjective-quality pass. The earlier diagnostic Level 1 win
remains valid within its separately stated scope.

The remaining gameplay checks are in the matrix above and the linked detailed
log. Audio signal or source correlation is not the same as listening acceptance.
Games with no authored audio playback should remain silent. Retail 64MB support
needs its own loading/performance tests before a compatibility claim.

For review, the latest already-merged milestones are
[color sensing/Breakout #22](https://github.com/ZacharyLeahan/scratcheverywhere-nxdk/pull/22),
[normal deployment/Tetris #23](https://github.com/ZacharyLeahan/scratcheverywhere-nxdk/pull/23)
and [six-pipe Ultra Bird #24](https://github.com/ZacharyLeahan/scratcheverywhere-nxdk/pull/24).
This document is a draft handoff for Zachary to share; it has not been sent to
Mike or posted to Discord.
