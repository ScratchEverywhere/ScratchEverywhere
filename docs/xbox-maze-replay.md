# Maze Starter replay

Maze Starter's Goal runs a forever loop which says “You win!” when touching
Ball. Ball has a separate Green Flag position reset. On the Xbox, replay after
winning reset Ball but left the win bubble visible. The runtime clears speech
during replay, but Goal can recreate it before Ball's reset runs. With no
“say empty” branch after the sprites separate, that newly created bubble remains
at the start of the next game.

Prepare a separate copy with:

```sh
python3 scripts/prepare-maze.py ORIGINAL.sb3 PREPARED.sb3
```

The script inserts a zero-second wait between Goal's Green Flag hat and its
collision loop. The wait yields one scheduler turn so Ball can reset first.
It preserves all other blocks, assets, credits and controls, and refuses to
overwrite an existing output. Keep the existing arrow/D-pad/stick sidecar (or
[the arrow-game profile](../examples/arrow-game-xbox-controls.json)) beside the
deployed archive, using the deployed archive's exact filename plus `.json`.

This is a project startup-order repair. The runtime already clears speech during
Green Flag. Scratch VM likewise clears bubbles when stopping and starts hats
using reverse executable-target order; see its
[runtime](https://github.com/scratchfoundation/scratch-vm/blob/develop/src/engine/runtime.js)
and [looks blocks](https://github.com/scratchfoundation/scratch-vm/blob/develop/src/blocks/scratch3_looks.js).
No global scheduler or speech behavior is changed.

The original download has SHA-256
`999c9aa6db87b11abcbd93a97ea514dbeed59e2e9390b8199fa0eadd69ed19fe`.
The prepared archive has SHA-256
`d25ede132506ab2cae5eb5bbd9f5be83fef150ceb5ab2b25f03b5ecde5510559`.
All three assets are byte-identical. Removing the inserted wait and restoring its
two adjacent links yields exactly the original project JSON. Overwrite and
changed-startup rejection were also checked.

## Physical Xbox verification, October 26

On build `26215225…`, the unmodified archive completed the full route, but
Green Flag reset Ball to the start while leaving “You win!” visible. The
prepared archive was then installed under the same browser filename and with
the unchanged D-pad/left-stick profile. The original archive is preserved as
`Maze-Original-20261026.sb3.fixture`, outside the browser's game extensions.
FTP readback matched the prepared archive.

The prepared game completed its full route and displayed a new win message.
Green Flag then returned Ball to the start with the old bubble gone.

A second complete route produced a second win, and a second Green Flag replay
again reset Ball without a stale bubble. Exit returned to the populated game
list after A release. These checks used the diagnostic build with bounded menu
input tracing enabled; a normal-build follow-up is recorded separately.

On normal build `be4bc0b4…`, a fresh browser launch, third full-route win,
Green Flag replay with a clear start screen, and exit to the populated list
also passed. The same prepared archive and control profile were verified again
by FTP before this run. No sound blocks or sound assets are authored in this
project. These successful exits do not resolve the separate intermittent
Catch-to-2048 browser handoff defect.

Ignored local evidence in `build-xbox/hardware/` includes
`maze-replay-{preparation,deploy}.json`, `maze-combined-route-end.jpg`,
`maze-combined-replay.jpg`, `maze-fixed-{first,second}-route-end.jpg`,
`maze-fixed-{first,second}-replay.jpg`, `maze-fixed-post-release-browser.jpg`,
`maze-normal-fixed-{route-end,replay,browser}.jpg`, the corresponding route
position logs, and `maze-prepared-{trace,normal}-runtime.log`.
