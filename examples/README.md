# Xbox Scratch smoke test

Generate the small offline Scratch 3 project:

```sh
python3 examples/make-xbox-test.py
```

Copy the resulting `Xbox-Test.sb3` to `E:\ScratchEverywhere` on the Xbox and
select it in Scratch Everywhere's project browser. It uses only SVG costumes
and standard Scratch blocks; no network access or external assets are needed.

The orange square moves horizontally and bounces at the stage edge. With the
runtime's default controls, D-pad Up/Down move it vertically and A increases
its size. Left-stick Up/Down map to the equivalent arrow-key handlers.

The generated SB3 is ignored by Git. The generator is the source artifact.
See `docs/xbox-hardware-testing.md` for the observed real-console results and
their limits.


## Snake Xbox controls

For [Snake by NemoNaturally](https://scratch.mit.edu/projects/511466864/), copy
`snake-xbox-controls.json` alongside the downloaded game and name it
`Snake-NemoNaturally.sb3.json` in `E:\ScratchEverywhere`.
The profile maps the D-pad to arrow keys and the left stick to WASD, both of
which the original game supports. A remains a confirm button in the runtime
pause menu without also sending Snake's A/left steering key. Use the D-pad
inside that pause menu. Returning to the project browser restores its default
controls. The game itself is not included or modified.

### Multiple controller inputs for one Scratch key

Control profile values may be a button name or an array of button names.
For example, `"up arrow": ["dpadUp", "LeftStickUp"]` binds both inputs to
the same Scratch key. Existing string-valued profiles continue to work.
`arrow-game-xbox-controls.json` maps both directional inputs to arrows and
A to Space, suitable for arrow-controlled games such as Tetris. Copy it beside
the game as `<game filename>.json`. These profiles require the updated runtime.
