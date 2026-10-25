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
