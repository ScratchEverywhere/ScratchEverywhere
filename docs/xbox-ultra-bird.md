# Ultra Bird external-project qualification

The installed `Ultra-Bird.sb3` matches the original Discord example byte for
byte: 22381 bytes, SHA-256
`f848b1d517b3da4d073971aa2ec8b310a64f7d039cb24e1fc25ea9c3e8decc52`.
It needs no controller sidecar: the bird's `when any key pressed` hat accepts
the default A binding. The game archive and runtime were unchanged in this
qualification run.

## Current normal-build gameplay

Normal XBE `07f15fc60186e719c48834596bbae080b0bc4d283c27af067806b3634c9ee3b4`
selected the external archive from the regular project browser on the 128 MB
Xbox. A visibly flapped the bird, and video-guided ordinary controller inputs
reached Score 2 in one round and Score 1 in another. These are current-build
checks, separate from the older embedded-build Score 2 result.

Local evidence is under ignored `build-xbox/hardware/`:
`ultra-selected.jpg`, `ultra-current-launch.jpg`,
`ultra-pilot5-round2-loss.jpg` (Score 2) and
`ultra-pilot5-round4-loss.jpg` (Score 1). The source audit and complete archive
readback are `ultra-source-audit.json` and `ultra-current-Ultra-Bird.sb3`.

The game broadcasts `Lost`, whose receiver in sprite G executes stop-all.
It does not author a separate Game Over panel. The runtime removes the pipe
clones on stop, leaving the score and bird's last position; the bird can be
off-screen. That appearance alone is not evidence of an application hang.

The bounded pilot attempts used one continuous KVM viewer, local JPEG reads,
100 ms raw controller refreshes and cached WebSocket route status. They did
not directly poll the S3 HTTP servers or write game state. Early pilots had
imperfect loss detection and flap timing. `ultra-pilot4` also failed in the
test helper's progress logging and released its controller; that attempt is
excluded from gameplay acceptance. A subsequent single-flap observation rose
from about screen Y=390 to Y=322.5, showing approximately 68 pixels of lift.
This is a visual timing observation, not proof of the exact input transport
latency or absence of missed/duplicate reports.

The replay and browser-return checks below are independent of the pilot's
automatic loss classification. Sustained flight beyond two scored pipes
remains open, as does the shared [KVM delivery fault](xbox-kvm-input-delivery.md).

## External short-Start replay

A separate, manually inspected check began from the stopped Score 1 round.
A 250 ms Start press restored Score 0, the bird and newly moving pipes. A fresh
120 ms A press then visibly lifted the bird as the pipe approached. A subsequent
6.5-second Start hold opened the runtime pause menu. This establishes the
short-Start replay shortcut for an external browser project on the current
normal build, rather than relying on the original embedded-project check.

Captures are `ultra-current-loss-score1.jpg`, `ultra-current-replay-zero.jpg`,
`ultra-current-replay-flap.jpg` and `ultra-current-pause.jpg`. The bounded input
receipts are retained in `ultra-menu-replay-1-client.json`.

Exit with a short A confirmation returned to the populated browser. Ultra Bird
was selected from that list again, launched at zero, and two fresh A presses
raised the bird as new pipes advanced. Long Start opened pause again.
Captures are `ultra-current-exit1.jpg`, `ultra-fresh-selected.jpg`,
`ultra-fresh-launch.jpg`, `ultra-fresh-flap{1,2}.jpg` and
`ultra-fresh-pause.jpg`. This repeat uses the normal browser and archive;
it is not a direct-load or debugger-seeded test.

The second short-confirmation Exit also returned to the populated list
(`ultra-current-exit2.jpg`). The retrieved `ultra-current-runtime.log` records
two Ultra Bird loads, three browser activations, four cleanup messages,
18 stopped-project restart markers across the diagnostic attempts and the
dashboard reset. Those 18 markers are not 18 accepted flight runs; the separate
scored-loss/250 ms Start captures establish the replay acceptance above.
Final FTP readback reconfirmed the unchanged archive, and the console returned
to XBMC with all test controllers released. These short-confirmation passes do
not resolve the independently reproduced held-confirmation KVM handoff failure.

## Authored audio

All seven targets contain zero sound assets and no `sound_` or `music_` blocks.
Silence is therefore expected for this archive, rather than a missing Xbox
audio implementation. `ultra-source-audit.json` records this inspection of the
actual installed bytes. Audio stability in games that play sound remains a
separate requirement in the [twenty-game matrix](xbox-game-qualification.md).
