# Xbox menu confirmation and game input

Connect Four exposed two input transitions where holding A could change its
authored control mode before the player made a gameplay input. The browser
started a small archive before A was released. Pause also restored the game's
A-to-Space profile immediately after Green Flag confirmation. In addition,
menu polling updated game key hats and queued scripts while gameplay was paused.

Xbox browser launches now defer the selected project load until A/X and the
mouse confirmation are released. Xbox pause actions similarly wait for release
before resume, Green Flag, Stop or Exit. Other platforms retain immediate
actions. Paused Xbox input still updates held durations for UI navigation, but
does not queue key hats, Makey Makey sequences or sprite clicks for gameplay.

## Focused runtime fixture

```sh
python3 examples/make-pause-input-test.py /tmp/Pause-Input.sb2
```

Place the generated adjacent `.sb2.json` profile beside the archive. It maps
A to Space, X to X, B/Y to themselves, and the D-pad/stick to arrows.
The fixture's FLAGS counter increments on every Green Flag. Game-event counts
must remain zero after menu navigation, held A/X replay confirmation and B/Y
resume. A new A press during gameplay must increment SPACE, establishing that
game input still works. This fixture is not a qualification game.

The pause change passed in the actual Xbox build in isolated xemu. B resume
after navigation left all event counts zero. Held A and X confirmations each
incremented FLAGS once, from 1 to 2 to 3, without incrementing SPACE/A/X/NAV.
A fresh 300ms gameplay A press then incremented SPACE to 1, and a held Y resume
left that count unchanged and Y at zero. GDB read back FLAGS=3, SPACE=1 and
A/B/X/Y/NAV=0. Evidence is in ignored `build-xbox/pause-input/`, including
`fixture-final-state.json`, framebuffer captures, matching symbols and build
metadata. This run used a GDB-only network initialization bypass.

The same build replayed the prepared Connect Four game with A held 2.2 seconds.
The board restarted with player 1, red, keyboard mode, and six alternating
red/yellow drops filled a column. A seventh drop was rejected without advancing
the player. Continued play completed a horizontal red win. Evidence is in
`build-xbox/pause-input/connect-four/`. One long controller batch hit the CUA
tool timeout; the actual board was inspected before sending only the remaining
moves. The timeout is not classified as a game failure.

## Faster GDB project selection

`scripts/xbox-open-project.py` registers the GDB command
`xbox-open-project D:/p.sb2`. Stop at the initial `Unzip::detectFormat`, source
the command, set a short DVD-root path, then continue for normal archive and
profile loading. This avoids rebuilding the executable for each test archive.
The command requires matching 32-bit little-endian symbols and an empty
`Unzip::filePath`; it refuses heap strings or paths beyond libc++'s short-string
capacity. It was verified with both the event fixture and prepared Connect Four.
It changes debugger state only; no direct-open path is compiled into the game.

## Physical Xbox checks

The combined candidate `0c7efd081c5615aba4d81ee2db4596e226b8aa2ae3c7acb1550080eee8b95ad1`
(9,687,040 bytes) was uploaded to `F:\\scratch-menu-input-20261026` and fully
read back. Its first held-A browser launch still showed mouse mode/player 2
after release; this observation remains unresolved. After manually restoring
keyboard mode, Green Flag with A held 2.5 seconds preserved keyboard mode and
red first. Subsequent red D-pad and yellow right-stick drops landed correctly.
A held Exit returned to the browser without an accidental launch. A repeated
browser launch with A held 2.2 seconds then started in keyboard mode, red first.
The matching final build also kept neutral keys/buttons at initial game startup
in xemu after a five-second A browser confirmation; mode remained zero. The
emulator used the original unprepared archive, whose separate authored
initialization bug leaves player zero; this is not a prepared-game pass.

Hardware evidence is `build-xbox/hardware/menu-input-*.jpg`,
`menu-input-deploy.json` and `menu-input-observations.json`. The exact cause of
the first hardware launch observation is not established; one repeated pass
does not establish reliable launch behavior. The older regular Games launcher
has not been replaced.

The repeated prepared-game hardware run filled column one with six alternating
red/yellow pieces. A seventh red drop was rejected and player 1 remained active.
Continued play completed a horizontal red win across bottom columns 1–4, with
yellow pieces in columns 5–6. The visible result is retained as
`menu-input-connect-horizontal-win.jpg`. The prepared project contains sound
assets but no authored sound-playback blocks were found; silence is not treated
as an audio-backend failure.
