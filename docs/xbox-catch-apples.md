# Catch Apples controls and replay

The Xbox install was missing `Catch-Apples-superd1.sb2.json`. The game checks
`left arrow` and `right arrow`, while the default D-pad keys are `g` and `j`.
The default left stick already supplies arrows. This explains the observed
stationary basket during D-pad testing; it is separate from the authored
timer-expiry behavior.

Copy [the supplied profile](../examples/catch-apples-xbox-controls.json) next to
the game archive, naming it exactly `Catch-Apples-superd1.sb2.json`. It binds
both D-pad and left stick to the game's left/right arrows. No archive edits or
runtime changes are required. Hold Start for the runtime menu; after timer
expiry, a short Start press restarts the stopped project.

## Physical Xbox check, October 26

On synchronized build `be4bc0b4…`, the initial D-pad run left the bowl at
approximately screen X=112 throughout. It reached Score 9 through incidental
stationary catches. The retrieved log reported that the control sidecar could
not be opened. An FTP folder audit confirmed the file was absent.

The new profile was uploaded with complete readback verification, SHA-256
`19a523f3f4501309d929545ab4edef0f529d218176ab333b941846344aaac04e`. Repeating
the timed round with D-pad steering moved the bowl across the stage and reached
**Score 22 / Timer 0**. The complete Game Over/replay instruction remained
visible. This ending uses Scratch stop-all; it is not evidence of an application
hang.

A separate 500 ms Start press restored **Score 0 / Timer 30**. Left-stick Left
then moved the bowl left, and Left-stick Right moved it back right as the timer
advanced through 29 and 28. An eight-second HDMI capture from the earlier round
contains non-clipping audio (peak 0.5, RMS 0.013642); subjective and long-run
audio quality remain unverified.

Exit returned to the populated browser after confirmation was released. A fresh
browser launch showed another playable round; D-pad Right moved the bowl right, and Left
moved it back left. The second exit also returned to the list.
Bonus/rotten-apple scoring has not been isolated in a targeted test. The game
scripts and assets remain unchanged. The installed archive matches the original
download byte for byte, SHA-256
`dfdc129664dff3a9b65f1c3523ba5d05f162e0b198b474cccc2cfd58cf7c1ede`.
The retrieved `catch-fixed-runtime.log` confirms all four movement bindings,
short-Start replay, two cleanup/browser returns and the dashboard reset combo.

Local evidence is under ignored `build-xbox/hardware/`:
`frog-snake-catch-controls.log`, `installed-profile-audit.json`,
`catch-sync-inputs.json`, `catch-sync-gameover-before-controls.jpg`,
`catch-controls-deploy.json`, `catch-fixed-inputs.json`,
`catch-fixed-score-22.jpg`, `catch-fixed-short-start.jpg`,
`catch-fixed-stick-{left,right}.jpg` and `catch-sync-audio.{wav,json}`.

Fresh-launch movement evidence: `catch-fixed-fresh-{right,left}.jpg`.
Earlier files named `catch-fixed-exit-browser.jpg` and
`catch-fixed-second-exit.jpg` captured the pending pause-menu confirmation,
so they are not browser-return evidence.

An additional October 26 exit on the same build unexpectedly launched 2048
instead of remaining in the list (`catch-extra-unexpected-2048.jpg`). The
retrieved `catch-extra-unexpected-2048.log` confirms Catch cleanup followed by
a 2048 load. Thus the earlier two successful returns do not establish reliable
menu handoff. This intermittent defect remains under investigation.

The separately deployed bounded-trace build (`26215225…`) then passed three
Catch exits and one Snake exit, including Snake followed by Catch. The retrieved
`catch-menu-handoff-trace.log` records neutral input at each of those exits.
It also records a transient A release/reappearance during one held launch
confirmation (158318–159284 ms), so a single observed release does not always
mean the KVM-held button has finished. The failing exit itself was not traced;
the cause remains unproven. Post-release browser captures are
`catch-trace-post-release.jpg`, `catch-trace2-post-release.jpg` and
`catch-trace3-post-release.jpg`.
