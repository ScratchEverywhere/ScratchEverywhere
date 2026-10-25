# Ten-game hardware qualification

The goal is ten playable games, with actual Xbox testing and fixes until each
works. Downloads, successful builds, and a title screen are not full passes.
The current console has 128MB RAM; this does not establish retail 64MB support.

For each game, establish selection from the normal browser, correct visuals,
controller movement and actions, a meaningful gameplay objective/score, loss or
time expiry, replay, pause/exit, and another successful launch. Check collision
behavior where relevant and listen to audio before claiming audio acceptance.
Keep failures and missing checks explicit. Use bounded controller inputs and
neutralize all reports afterwards.

| Game | Evidence so far | Still needed |
| --- | --- | --- |
| Hopper Heros Everywhere | Browser launch/exit; movement and 6 points in earlier tests | Repeatable gameplay, loss/replay, audio |
| Ultra Bird | Flap, loss, embedded Start replay; browser launch/exit; 2 points | External replay shortcut, sustained play, audio |
| Catch Apples | Browser launch; expiry at score 7; short Start replay resets timer/score; pause and exit to list pass | Repeat launch, directed collection/collision, audio |
| Maze Starter | Browser launch/exit; movement down initial corridor | Wall collision, goal completion, replay, audio |
| Snake | Four stick directions proven by input/position trace; browser launch/exit | Clean D-pad/profile test, collection/scoring, loss/replay, audio |
| Appel v1.4 (griffpatch) | Packaged, uploaded and FTP readback verified | All hardware gameplay checks |
| Tetris (Tech-Coder) | Browser launch; D-pad movement/rotation, stick movement, A hard drop, top-out and Start replay | Line clearing/scoring, repeat launch/exit/audio; fix all-red pieces |
| Frogger (cs50) | Packaged, uploaded and FTP readback verified | All hardware gameplay checks |
| Atari Breakout (LVRS) | Packaged, uploaded and FTP readback verified | Mouse-emulation usability, all hardware gameplay checks |
| PacMan (cdaniels7088) | Packaged, uploaded and FTP readback verified | All hardware gameplay checks |

Sources, authors, instructions, packaging details and hashes for the additional
five are in ignored `build-xbox/more-games/next-five-manifest.json`. Game scripts
are unchanged; SB2 archives use numeric asset IDs assigned during packaging.
Upload readback results are in `next-five-installed.json` in that directory.
The browser visibly lists the new games; the first page shows Appel, Atari
Breakout, Catch Apples, Frogger and Hopper.

## October 25 continuation

Installed the multi-controller build in `F:\scratch-anypad-20261025`, SHA-256
`7059e259f086c5c6dd4503a14e36c4d0f997e2d8259d0fdd73380c82ef9a27cf`.
Player Three moved browser selection down to Hopper, Player One moved it back
up to Catch Apples, and Player Three launched Catch Apples. This verifies
switching between those two virtual controllers without relaunch. Physical-pad
switching and hotplug remain to be tested.

Catch Apples reached Timer 0 / Score 5. Holding Start opened the runtime pause
menu, so the reported freeze was not reproduced as an application hang in this
run. Its scripts intentionally stop all at timer expiry. A short-Start replay
shortcut is being tested to make recovery discoverable without the pause menu.
The next build, `F:\scratch-replay-20261025`, has SHA-256
`16895eb85396fbbe16142fde0baf05a2c6aad69b621eeed57e274f22465fd600`.
It also accepts array-valued control bindings, with arrow-game profiles installed
for Appel, Tetris, Frogger and PacMan. Those profiles map both D-pad and left
stick to arrows, and A to Space. Atari Breakout currently uses the existing
shoulder + D-pad mouse emulation and still needs a usability assessment.

With that replay build, Catch Apples reached Timer 0 / Score 7. A 250ms Start
press on Player One started a fresh round; the next capture showed Timer 25 /
Score 1. Holding Start on Player Three opened pause during that round, and A
exited to the browser. Evidence: `catch-replay-stopped.jpg`,
`catch-replay-after.jpg`, `catch-replay-pause.jpg`, `catch-return-list.jpg`.

Tetris was selected from the scrolled browser and launched. After top-out,
250ms Start produced an empty board with a fresh L piece. D-pad Up rotated it
to a vertical L and Player One's left stick moved it to the right wall. A
hard-dropped the following piece and a new piece spawned. This exercises array
bindings on the D-pad and stick, string bindings on A, and live controller
switching. Evidence: `tetris-selected.jpg`, `tetris-launch.jpg`,
`tetris-replay.jpg`, `tetris-rotation-stick.jpg`, `tetris-hard-drop-live.jpg`.

Some intermediate Tetris still captures were stale after the old 180-second
stream expired; they are not acceptance evidence. The capture helper was
extended to a persistent one-hour stream, and live status showed one viewer
and a frame age of 13ms before the replay/rotation/stick/hard-drop checks were
repeated. Keep a live stream and verify freshness during later tests.

All Tetris pieces render red. The project uses color effects; the Xbox renderer
currently does not forward/apply `Sprite::colorEffect` as the OpenGL core
renderer does. Treat this as an unresolved compatibility defect, not a full
game pass. No line-clear/scoring or audio acceptance is established yet.
