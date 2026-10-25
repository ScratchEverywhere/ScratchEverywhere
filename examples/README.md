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
