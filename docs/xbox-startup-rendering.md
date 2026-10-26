# Intermittent Xbox startup rendering

## Reproduction and isolation

On the 128MB hardware console, some dashboard launches reached the browser but
showed only its purple clear color. Selecting Appel could similarly show only
black. Input, pause, project parsing and the expected clone populations still
worked. This failure was separate from Appel's empty-condition clone overflow.

A startup pixel probe drew a red untextured triangle and a green texture over a
blue clear, then read the GPU framebuffer. In a failed boot both shape samples
remained blue, despite successful SDL draw calls. A successful boot returned the
expected red and green. The arena physical address was 0x034d8000 in both cases,
and the framebuffer address was also identical. This ruled out project assets
and the earlier above-64MB GPU-allocation issue as explanations for this fault.

Controlled changes within a failing boot established:

- Explicit current position defaults and four-component positions did not help.
- Disabling alpha/stencil/fog tests and resetting the depth clip range did not help.
- `NV097_BREAK_VERTEX_BUFFER_CACHE` did not help (`cache-cycle-6.log`).
- Loading a three-instruction pass-through vertex program and switching from
  fixed-function to programmable processing restored both shapes immediately,
  without restarting the console or reloading the project
  (`transform-cycle-3.log` and `cache-cycle-6.log`).
- A separate material-alpha 0/1 comparison left successful drawing unchanged.

The experiments isolate the unreliable fixed-function vertex path. They do not
identify a particular retained transform-context register. The repair removes
that dependency rather than claiming a specific unmeasured register defect.

## Repair

The SDL XGU dependency patch installs an explicit screen-space vertex program
at renderer creation. It passes through position, vertex RGBA and texture
coordinates, adding a viewport-offset constant to position. That constant is
initialized and updated on viewport and render-target changes. The existing
GPU allocation bounds, fragment combiners, SDL scaling and texture formats are
preserved.

The program's assembly is included alongside its generated instruction words
in the patch. Those words were verified against the project-local NXDK
`vp20compiler` output. Applying the full patch to a clean copy of its pinned
upstream source produced byte-identical copies of all five modified dependency
files. For an existing Catalog cache, configure once with
`-DCL_REFRESH_SDL3=ON`, then remove the setting with `-UCL_REFRESH_SDL3`.

`SE_XBOX_TRACE_RENDERER=ON` enables an optional pixel regression probe and SDL
logging to the normal Xbox log. It checks solid/textured drawing, viewport
translation and clipping, render-target restoration, and alpha modulation.
The probe preserves the application's viewport and scale; it defaults to OFF.
Temporary register dumps and experimental repair branches are not included in
the final application.

## Hardware verification

The final renderer with its probe enabled passed all eight checks on hardware:
solid, textured, clear, viewport offset, viewport outside, render-target restore,
render-target outside, and alpha modulation. The diagnostic executable SHA-256
is `fadb98f3ce2650656e4e2cc4e43c27e14f1ff021a77414ac557f16d3d4a80f73`;
proof is `screen-space-probe-cycle-1.log`.

The first production executable had both renderer and gameplay tracing disabled
and was installed in XBMC's configured Games source at
`F:\Games\ScratchEverywhere\default.xbe`. Its complete FTP readback matched
SHA-256 `3e68d3a127afdd2fe0ac99d74c54177ea5956e9b40ef59eaf7f6898d92b95fe1`
(9,465,856 bytes). Production launch results are saved separately in
`final-startup-results.json`; experimental repair boots are not counted.

**Eight consecutive dashboard launches passed** with diagnostics disabled.
Each reached a visible browser and remained visible for a further 6.2 seconds
before capture/reset. All eight saved screenshots were also reviewed together
(`final-startup-contact-sheet.jpg`). These are warm dashboard launches on this
128MB Xbox at 480p, not eight physical power-cycle tests. The final browser then launched the full Appel project successfully. Its title
and level terrain rendered, right movement and jumping worked, and an apple
was collected. Holding Start opened the runtime pause menu; Exit Project
returned to the visible browser without rebooting. Evidence is
`startup-final-appel-title.jpg`, `startup-final-appel-level.jpg`,
`startup-final-appel-moving.jpg`, `startup-final-appel-exit-menu.jpg`, and
`startup-final-appel-browser-return.jpg`. This checks the launch/game/exit path;
it does not establish full-level completion.

Ignored proof files are in `build-xbox/hardware`. Baseline screenshots and logs
are `startup-cycle-{1,2,3}.*`; cycles 1 and 3 were visible and cycle 2 was blank.
The initial automation sampled text ink and mislabelled the visible browsers;
`startup-baseline-summary.json` corrects that classification from the saved
screenshots. Geometry experiments are in `geometry-cycle-3.log`; the successful
same-boot program repair is in `transform-cycle-3.log`; cache invalidation's
failure followed by program recovery is in `cache-cycle-6.log`; material-alpha
controls are in `alpha-cycle-1.log`.

## Combined-build startup recheck

Later on October 25, the installed combined color-sensing/SB2-costume build was
retrieved in full from `F:\Games\ScratchEverywhere\default.xbe`. Its SHA-256
matched `60dc7891023a9c476d1f5f19dd943ade8e1abbdd551a91aa2dd34eb29a77df57`
(9,469,952 bytes). This build retains the vertex-program fix and has both
renderer and gameplay tracing disabled. The local work-in-progress color-effect
executable was not deployed for these checks.

**Four additional consecutive dashboard launches passed** on that installed
build. The live HDMI feed had an active viewer and a frame age under one second
before each accepted observation. Each visible browser persisted for at least
6.2 seconds; captures occurred 14.4, 20.5, 20.5 and 31.9 seconds after monitoring
began. All four screenshots were reviewed together. These remain warm launches
on the 128MB console at 480p, not physical power-cycle tests.

Evidence is in ignored `build-xbox/hardware/startup-recheck-results.json`,
`startup-recheck-{1,2,3,4}.jpg`, `startup-recheck-contact-sheet.jpg`, the retrieved
startup logs, and `startup-recheck-installed.json`. The retrieved executable is
preserved as `startup-recheck-installed.xbe`.

The follow-up Appel selection was visually confirmed, but its launch is **not
accepted** in this recheck: capture showed horizontal colored stripes over
black, including after a five-second Start hold. During this interval the KVM's
frame/mode counters reset and its system information reported 145 seconds of
uptime. After the controller reset attempt, Xbox FTP was unreachable
(`Host is down`), so an Appel log could not be retrieved. This does not identify
the striped image's cause or establish that it is the old clear-only vertex
failure. Physical display/power confirmation is required before further
hardware work. The misleadingly named `startup-recheck-appel-title.jpg` records
the stripes, not a successful title; `startup-recheck-appel-loaded.jpg` and
`startup-recheck-appel-pause-attempt.jpg` show the same result. KVM status and
log snapshots are saved alongside them. No XBE or project was replaced during
this recheck.


## Physical recovery follow-up (October 26)

The user confirmed that the physical display also showed colored stripes or
black, so the failed image cannot be dismissed as capture corruption alone.
After a physical power cycle to XBMC, FTP recovered. The retrieved log
(`startup-recheck-appel-recovered.log`) instead names
`Atari-Breakout-LVRS.sb2`, reaches texture precache completion, and has an FTP
modification timestamp of `20261026021112`. It is preserved, but cannot be
attributed to the earlier visually selected Appel attempt. It supplies no
Appel-specific failure diagnosis.

A fresh launch of the unchanged installed executable reached a visible browser.
Appel was selected and captured before confirmation
(`recovery-appel-selection.jpg`); its title then rendered correctly
(`recovery-appel-launch.jpg`). The live KVM had one viewer and frame age below
one second. This successful recovery does not establish the striped failure's
cause or qualify physical cold-start reliability.

The same recovery run entered level one, rendered terrain, and reached two
apples after right movement and jumping (`recovery-appel-motion.jpg`). A
five-second Start hold opened the runtime pause menu
(`recovery-appel-exit-menu.jpg`). These observations used the unchanged
`60dc7891…` installed build.
Exit Project then returned to the visible game browser without rebooting
(`recovery-appel-browser-return.jpg`).

The later color/pen production candidate also rendered Appel's title and level
four terrain, accepted movement/jump, and returned from pause/Exit Project to
the browser. Its promotion to the regular launcher reached another fresh
visible browser. See [the color-effects record](xbox-color-effects.md) for
that separate compatibility test and installed hash. Neither this success nor
the old-build recovery establishes the cause of the striped event.
