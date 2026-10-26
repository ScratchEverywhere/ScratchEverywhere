# Hopper Heros controller qualification

The author's `Hopper-Heros-Everywhere.sb3` is unchanged, SHA-256
`0eb1e25bef8850ffce783888d74d82add0e4718873beda01fcc33a44a90e9db8`.
It supports keyboard steering with Left/Right arrows, A for jumping, 1 for
Play/Retry, B for its main menu and 0 for returning from its shop.

The initial Xbox install had no adjacent controller sidecar. The default left
stick supplies arrows, but the default D-pad supplies other letters. Copy
[the supplied profile](../examples/hopper-xbox-controls.json) next to the
archive as `Hopper-Heros-Everywhere.sb3.json` to bind both D-pad and stick to
arrows. A remains the game's A key, Start remains 1, and B/Back retain their
authored actions. All 24 physical bindings are unique, and every explicit key
reporter in the project is covered. No game scripts or assets need changing.

## Normal-build hardware checks

Build SHA-256 `07f15fc60186e719c48834596bbae080b0bc4d283c27af067806b3634c9ee3b4`
launched Hopper from the populated browser. Before installing the profile, a
short Start entered gameplay, the first round reached a Points Gained 3 loss
screen, and short Start retried with a visible score of 0. The second round
ended at 1 point. Those pre-profile D-pad commands are not movement acceptance:
they use the default letter mapping.

The missing-sidecar warning is preserved in
`build-xbox/hardware/hopper-before-profile-runtime.log`. Captures include
`hopper-round1-loss-score3.jpg`, `hopper-round2-start.jpg` and
`hopper-round2-loss-score1.jpg` in the same ignored directory.

The profile was installed with full temporary-file FTP readback before rename,
SHA-256 `550640390d80e61ddc3ad15eaf8def76903ef1d2e459a074c92d2db65c34ff5e`.
`hopper-profile-deploy.json` records the deployment. Post-profile gameplay,
exit/relaunch and audio are separate checks from successful installation.

With the profile installed, D-pad Left moved the player left of center and
Right moved it back right. Left-stick Left reversed the preceding rightward
movement, and Right moved the player toward the opposite edge. A created the
jump cloud and lifted the player relative to the scrolling field. These inputs
used ordinary game scripts and controller reports, without debugger writes.
The round ended at 3 points. Start restored zero, another played round ended
at 8, B returned to the authored menu, and Start entered another zero-score
round. A 6.5-second Start hold opened the runtime pause menu; a short A on Exit
returned to the populated browser.

Movement/action captures include `hopper-profile-{left,right,jump-right}.jpg`,
`hopper-profile-{stick-left,jump-stick-left,stick-right,jump-stick-right}.jpg`,
`hopper-profile-loss.jpg` (3 points), `hopper-profile-loss8.jpg`,
`hopper-profile-retry-zero.jpg`, `hopper-profile-b-menu.jpg` and
`hopper-profile-menu-play.jpg`. The earlier filename `hopper-profile-loss5.jpg`
also shows 8; 5 was an intermediate live score, not the final loss result.

An eight-second stereo HDMI sample contains 384000 frames at 48 kHz, peak
0.0611 and RMS 0.01691, with no clipping. Evidence is
`hopper-profile-audio.{wav,json}`. This establishes an audio signal, not
subjective quality or long-run audio stability. Longer sustained progression
and the shared [KVM delivery fault](xbox-kvm-input-delivery.md) remain open.

A fresh browser launch reached the title and exited again, but its early
200 ms Start command did not enter gameplay. The captures named
`hopper-fresh-{zero,left,jump,right}.jpg` all still show the title and are
excluded from movement/replay acceptance. Another fresh launch, followed by
visual confirmation of the ready title and a 500 ms Start, did enter gameplay
at zero. D-pad Left, A jump and Right worked again; valid captures are
`hopper-fresh-retry-{title,zero,left,jump,right}.jpg`. These results do not
isolate whether the missed early command was startup timing or delivery loss.

All three post-profile launches exited to the populated browser using short
A confirmations. The third followed the successful fresh-launch gameplay
repeat. Evidence is `hopper-profile-exit{1,2,3}.jpg`. These three passes do not
resolve the separately reproduced held-A handoff failure. The final retrieved
`hopper-profile-runtime.log` records three successful profile loads, all
steering/action bindings, six cleanup messages, four browser activations and
the dashboard reset. Final FTP readback confirms the original archive and
installed profile hashes.
