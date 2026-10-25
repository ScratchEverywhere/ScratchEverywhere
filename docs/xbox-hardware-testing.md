# Original Xbox startup investigation

## October 25, 2026: first hardware launch

The default build with upstream automatic video mode selection was deployed to
`F:\scratch-everywhere-test-20261025\default.xbe` through XBMC FTP. Its SHA-256
was verified by reading it back before launch:
`f983cf5b0d3f77a4129a45a9f5a7aaa1ee72bcd910ac6ab3438bc374f7dceb36`.

ESP-KVM captured OS startup, detection of 128 MB RAM, then a solid purple
screen. A bounded A press did not visibly change that screen. An explicitly
approved HDMI capture reconnect restored signal lock but showed the same
picture. The operator subsequently returned the console to XBMC.

The retrieved `E:\ScratchEverywhere\debug.txt` established that network
initialization succeeded and the program reached "Main Menu activated."
It reported an HDTV component cable and a selected 1280x720 video mode, while
capture remained 720x480. Thus the initial evidence does not support an audio
initialization hang; actual rendering and video mode selection need checking.

Local evidence, excluded from Git, is under `build-xbox/hardware/`: deployment
manifest, startup log, settings, and screenshots. The original XBE is retained
as `original-default.xbe` there.

## Diagnostic candidate

`SE_XBOX_FORCE_480P=ON` disables the automatic 720p selection without changing
the console EEPROM. Additional startup logging reports the SDL renderer output
size and active NXDK video mode. Build with:

```sh
bash scripts/build-xbox.sh -DSE_XBOX_FORCE_480P=ON
```

Deploy diagnostic builds to separate folders and verify FTP readback before
launch. A successful compile or "Main Menu activated" log is not visual
acceptance. Confirm the logo, menu controls and a response to controller input.

The 480p candidate compiled and packaged successfully (Clang 23.1.2). XBE
SHA-256: `42a87ecc8429f5623b438af6176d8e651da5ef958889842d7d73250585334ab5`.
It was uploaded to `F:\scratch-everywhere-480p-20261025\default.xbe`, verified
by FTP readback, and launched. Both documented ESP-KVM addresses then timed
out, preventing visual verification. The actual console picture and the
candidate's startup log were not available at that point. The operator then
confirmed the Scratch Everywhere menu on the physical display.

## Controller and Scratch project test

The operator returned to XBMC and closed extra viewers. The generated
`examples/Xbox-Test.sb3` was uploaded to `E:\ScratchEverywhere` and verified
by FTP readback before relaunching the same 480p XBE.

KVM connections initially timed out; the device subsequently responded with a
recent uptime. No agent-requested firmware update or KVM restart was performed.
The older keyboard helper did not establish navigation. Tests then used the
current control-owned WebSocket raw-report protocol with bounded press times,
refresh and explicit neutral/release.

The player-3 route at the documented adapter address delivered held states and
incrementing Xbox USB completion counters. Continuous MJPEG capture established:

- Menu navigation into Settings and B return to the main menu.
- A opening the project browser, showing `Xbox-Test`, then launching it.
- The blue grid backdrop and orange square rendering on real hardware.
- Horizontal animation at different positions across captures.
- D-pad Up moving the square above the center line, and Down returning it.
- A increasing the square's size.

This is a small synthetic SB3 smoke test, not compatibility or performance
qualification for arbitrary Scratch projects. Audio was not assessed. The
other three routes did not establish visible navigation in this session; no
four-controller acceptance is claimed. One-off captures sometimes returned
plain purple frames; a bounded continuous viewer made transitions observable.
The project was left running with controller reports released.

## Hopper Heros embedded archive lifetime fix

Both Discord examples were uploaded and FTP readback verified. A later generic
runtime launch, including one after an operator cold boot, showed only purple;
its log still reached "Main Menu activated." Thus forcing 480p does not fully
resolve that separate menu issue.

A direct-start build embedding Hopper Heros bypassed the menu but showed black.
Its log stopped after the optional `project.sb3.json` settings warning. That
warning returns a null/default setting and is not itself a fatal error.

Code inspection found the Xbox embedded loader defaulted to a stream-backed
ZIP archive. Miniz retains the supplied stream pointer for later costume reads,
but `openScratchProject()` deletes that stream immediately after parsing the
project, before texture precaching. The embedded Xbox path now uses the
persistent `Unzip::zipBuffer` and the memory ZIP reader. Oversized embedded
archives fail with an explicit disk-loading recommendation instead of retaining
a dangling stream. External disk projects retain their existing behavior.
Checkpoints were added around archive loading, JSON/sprite parsing and precache.

The fixed NXDK build compiled, packaged, and passed FTP readback verification:
`0b1212f67eb55c9cfabe80903c8ea3cd373a45f4b6753987c696730170b733b5`.
Installed at `F:\scratch-hopper-fix-20261025\default.xbe`, it visibly reached
the Hopper Heros title. Start entered gameplay; a subsequent result screen
showed "Points gained: 6." Start/Retry and A input were also sent through the
previously verified port-3 route. This establishes title/gameplay progression,
not complete game or audio qualification. Screenshots are retained locally as
`hopper-fixed-title.jpg`, `hopper-fixed-gameplay.jpg` and `hopper-fixed-score.jpg`
under `build-xbox/hardware`.

The Discord game is not vendored in Git. To reproduce the direct build, obtain
the author's Hopper Heros SB3, place it at `romfs/project.sb3`, and run
`bash scripts/build-xbox.sh -DSE_XBOX_FORCE_480P=ON`. Remove that local file to
return to a generic runtime build.

## Ultra Bird direct-start test

The original Discord SB3 was embedded with the same archive lifetime fix and
480p option. The NXDK build passed and FTP readback matched
`210c31c1be2c60447b8c204be2fbd3ed2f69cf1ffa0dde0b808cfa9c46135353`.
Installed at `F:\scratch-ultrabird-20261025\default.xbe`, it displayed the bird,
pipe obstacles, scrolling/background artwork and score counter on the Xbox.

The bird's input script is `event_whenkeypressed` with `KEY_OPTION=any`.
Bounded A presses and two short repeated-tap sequences were sent through the
port-3 route with neutral/release afterward. Adapter status confirmed A and
Xbox USB completions. Captures showed the playfield changing after input, but
the bird remained near the floor and score stayed 0 in the saved samples.
Successful flapping, scoring and sustained flight were not established. This
is a boot/render pass with unresolved input/gameplay behavior, not a full pass.
Audio and the bitmap scaling issue mentioned by the author were not qualified.

Local evidence: `ultrabird-running.jpg`, `ultrabird-flap-*.jpg`, and
`ultrabird-deploy.json` under `build-xbox/hardware`. The original game was not
modified or added to Git. The direct-start build remains running for local
inspection, with all agent button states released.

## Ultra Bird Xbox restart and flap verification

Bounded input traces confirmed that Xbox A reaches the bird's `any` key hat
and sets its upward velocity. The original apparently unresponsive tests were
after Ultra Bird's `Lost` broadcast had executed `stop all`, removing its
movement threads. Embedded projects have no project-browser pause menu or
editor green flag, so the Xbox input backend now treats a fresh Start press
as green flag restart when an embedded project is stopped. While a project is
running, Start retains its existing Scratch key mapping.

Hardware Start logging confirmed embedded project type 1, stopped state 1,
and execution of `greenFlagClicked()`. A subsequent position/velocity trace
showed restart clearing the stop state and recreating seven threads. With A
inputs, the bird rose through y=42, 53, 61 and 76. At y=103, the game's own
stop-all condition fired again. This explains the later bird disappearance:
repeated flaps carried it above the visible stage before game over. It was
not a missing input or a stuck movement thread during active play.

The temporary per-frame and input diagnostic logs were removed from source.
Local evidence includes `bird-motion-check.log` and
`ultrabird-flap-confirmed.jpg` in `build-xbox/hardware`. The original author's
SB3 is unchanged. Scoring, sustained successful play, audio and bitmap
scaling remain unqualified.

The clean NXDK build was uploaded with FTP readback verification to
`F:\ultrabird-controls-20261025\default.xbe`, SHA-256
`3ae489c855273d02f91bf4fa3b4a8522091021ee2e158d26dbb510f93d4e5790`.
Controls: A flaps; after game over, release and press Start to restart.

## Ultra Bird scored gameplay

A later play session on the unchanged clean controls build reached a visible
score of 2 in one run on the Xbox. The saved capture is
`build-xbox/hardware/ultrabird-score-2.jpg` (local evidence, excluded from Git).
This confirms scored gameplay in addition to restart and flap delivery; it is
not a claim of complete game compatibility. Controller reports were neutralized
and KVM control released after each bounded attempt.

For reliable capture, keep a live video viewer active: the current KVM's JPEG
snapshot API can return the last published frame after a bounded continuous
viewer expires. Input timing based on those stale snapshots is unsuitable for
playing this game. A continuous viewer plus current-frame observation was used
for the scoring captures.

## Xbox project-browser launcher

A generic 480p build with `SE_MENU=ON`, `SE_LOADSCREEN=OFF` and no embedded
`romfs/project.sb3` displayed the main menu and all three installed projects.
The build's FTP readback SHA-256 was
`76a5994b4305c6f735bd3068aa9691526b7d8f91b07f7d6dee0c6ecbdf69a22f`.
Hopper Heros was selected and reached its title. Holding Start opened the
runtime pause menu; selecting Exit Project returned to the launcher. Ultra
Bird was then selected, launched and exited in the same process without a
console reboot. Local captures are `menu-check-main.jpg`,
`menu-check-browser.jpg`, `menu-check-hopper.jpg`, `menu-check-ultrabird.jpg`
and `menu-check-pause.jpg` under `build-xbox/hardware`.

The Xbox menu-enabled launcher now opens the installed project list directly
at startup and after exiting a project. It restores default navigation
controls before constructing the browser. Back still opens the existing main
menu, so settings remain accessible. An empty browser with no previous menu
now has a valid main-menu destination for A rather than clearing the active
menu. Standalone embedded builds retain their existing direct-game behavior.

This avoids making normal project selection depend on the introductory main
menu. The old intermittent purple main-menu screen was not reproduced in the
successful generic session above; its underlying cause has not been proven or
claimed fixed. Temporary main-menu frame checkpoints were removed.

Build the browser runtime with no embedded `romfs/project.sb3` or `romfs/project`
directory, then:

```sh
bash scripts/build-xbox.sh -DSE_XBOX_FORCE_480P=ON -DSE_MENU=ON -DSE_LOADSCREEN=OFF
```

Use D-pad Up/Down to select a game and A to launch. Hold Start for approximately
three seconds in a game, release it, then press A on Exit Project to return to
the game list. The pause menu also supplies Click Green Flag for restarting
external projects such as Ultra Bird.

### Upstream synchronization and direct-browser hardware acceptance

Merged upstream `main` through `c6ac49dd` without conflicts, including
`3982a765` (Xbox compiler flags), `50105c8a` (loading-block centering) and the
3DS pen-rendering fix. The existing Xbox changes were retained. The NXDK
browser build compiled and packaged successfully; the unsupported
`-falign-jumps` flag is absent from its generated application flags.

The synchronized code at `a7a23d49` was deployed to
`F:\scratch-browser-sync-20261025\default.xbe` with matching FTP readback:
`eef60dba16bb9168e33300c7b99030b148fad4de30be189dd9a8fb9d942b8834`.
On the real Xbox, this exact build:

- Booted directly to the installed project list.
- Launched Hopper Heros and displayed its title.
- Opened the runtime pause menu with held Start and exited directly to the list.
- Selected and launched Ultra Bird in the same process.
- Opened its pause menu and exited directly to the list again.

No reboot was needed between games. The console was left at the game list,
with neutral controller reports and KVM control released. Local captures:
`browser-sync-boot.jpg`, `browser-sync-hopper.jpg`,
`browser-sync-return-hopper.jpg`, `browser-sync-ultrabird.jpg` and
`browser-sync-final-list.jpg` under `build-xbox/hardware`.
This is a launcher/launch/exit acceptance pass, not a measured performance
comparison or renewed scoring/audio qualification. Animated loading remained
disabled, so the centering change was merged but not visually exercised here.
