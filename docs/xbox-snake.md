# Snake controller and gameplay checks

NemoNaturally's Snake was tested through the normal Xbox project browser on
synchronized build `be4bc0b4…`, installed separately in
`F:/scratch-sync-20261026/default.xbe`. The build passed FTP readback
verification. The game archive was not modified for these tests. Its installed
SHA-256 is `efd37742f496c4ad6e8baabac6d2ed0396def2f029de144e343f871340af7982`;
the installed profile SHA-256 is
`8f4aa042e9166fcf37f512779eefc9bcd2317ac7f60b3d39e4177c4e286f791b`.

## Physical Xbox, October 26

A bounded D-pad run lasted approximately 21 seconds and used all four
directions. The snake grew, collected points, and subsequently reached the
complete loss screen with **Score 125**. The downloaded project's existing
Highscore 2000 remained visible; that is not a result from this test.

Runtime pause and Green Flag replay produced a later loss screen with Score 0.
Exit returned to the populated browser after a 2.2-second A confirmation was
released. Selecting Snake again started a fresh game. Separate captures show Up, Left, Down and
Right moving the head during this fresh launch, followed by a second successful
exit to the game list. The prior up/down steering failure was not reproduced. An
eight-second HDMI capture contains non-clipping audio: peak 0.12561, RMS
0.012573. Subjective and long-run audio are not established by that sample.

The scoring run's temporary vision helper eventually confused the growing gray
snake/body with other gray objects. Its automatic "Game Over" message was
premature. The later actual Score 125 screen supplies the loss/scoring evidence.
The final tracking coordinates are not accepted as accurate head positions. The
fresh-launch directional captures independently establish all four D-pad
directions.

## Controls and remaining checks

Use the D-pad or the installed WASD-bound left stick. Hold Start for the runtime
menu, select Green Flag to replay, or Exit Project to return to the list. This
project's ending stops other scripts rather than using Scratch's global stop-all
state, so the runtime's short-Start stopped-project replay shortcut does not
apply. Green Flag replay is verified.

Explicit tail self-collision, clock/blade pickup behavior and longer audio/play
sessions remain unverified. The earlier input trace established the four stick
directions; the current run provides clean D-pad gameplay evidence. This is not
a full-board completion claim.

Evidence is under ignored `build-xbox/hardware/`: `snake-sync-score-125.jpg`,
`snake-sync-replay-score-zero.jpg`,
`snake-sync-fresh-{up,left,down,right}.jpg`,
`snake-sync-audio.{wav,json}`, `snake-auto2-inputs.json` and its intermediate
captures. A screenshot named `snake-sync-replay-active.jpg` still shows the menu
transition and is not used as evidence of active gameplay.

An additional fresh launch/exit captured the populated list after A release in
`snake-sync-post-release-browser.jpg`. Earlier files named
`snake-sync-exit-browser.jpg` and `snake-sync-second-exit.jpg` captured the
pending pause-menu confirmation; they are not browser-return evidence.

The final retrieved runtime log confirms profile loading, project cleanup and
browser return (`frog-snake-catch-controls.log`).
