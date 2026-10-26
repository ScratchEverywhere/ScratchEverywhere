# Scratch font compatibility on Xbox

Hill Climb Racing uses the Scratch 2 `Scratch` font for its countdown and
end-of-run messages. That family was absent from `Image::loadedFonts`, so
LunaSVG fell back to Noto Serif. Its wider glyphs extended beyond the authored
SVG viewBox: “Out of Fuel!” rendered as “Out of Fue”, and the countdown lost
its ending. This was separate from the SB2 text-position normalization.

The loader now registers `Scratch` with Scratch Savers, the metric-compatible
replacement used by TurboWarp. Existing SVG dimensions, costume centers,
scripts and archives are unchanged. This also makes the family available to
SB3 projects which reference it directly.

## Font source and conversion

- Source: [TurboWarp's font replacement](https://github.com/TurboWarp/scratch-gui/pull/909).
- Repository commit: `9c954186705af0d764d08aec86bad0c436c4829b`.
- File: `src/lib/tw-scratch-render-fonts/ScratchSavers_b2.woff2`.
- Source SHA-256: `abf9ee76b94a91c716119478bd87165ea4ec66c82ece817ed2d3730663bab389`.
- Bundled TTF SHA-256: `5ed5f649ea3cb6732420e82a17348fabf3f1b5b8c8149ca25f902dee3c047679`.

The source font is based on Life Savers and modified by valadaptive. Its
copyright notices and SIL Open Font License 1.1 are retained alongside the
font in `gfx/ingame/fonts/ScratchSavers-OFL.txt` and in the font's name table.
Only the WOFF2 container was converted, using an isolated FontTools 4.60.2
installation with Brotli 1.2.1. These are preparation tools, not runtime or
build dependencies. Preserve the original timestamp for reproducible output:

```python
from fontTools.ttLib import TTFont
font = TTFont("ScratchSavers_b2.woff2", recalcTimestamp=False)
font.flavor = None
font.save("ScratchSavers.ttf")
```

## Verification

Host LunaSVG 3.5.0 rendered the original Hill Climb assets after the existing
SB2 text conversion. The fuel message's horizontal bounds changed from
266.20 units to 226.44 units inside its unchanged 229-unit SVG. The countdown's
bounds ended at 244.42 units inside its unchanged 246-unit SVG. The complete
fuel message and countdown were visually inspected. Evidence is in ignored
`build-xbox/hardware/hill-fuel-before.png`, `hill-fuel-after.png`,
`hill-countdown-after.png` and `scratch-savers-source.json`.

The NXDK build passed. The separate test executable is
`F:\scratch-font-20261026\default.xbe`, 9,678,848 bytes, SHA-256
`fe4329c24ed1369d62d6644ff3600fdd0d51549e676816849e44f6dcac3be1e5`.
FTP readback verified the installed artifact, and the regular Games launcher
retained its previous hash. On hardware, `hill-font-countdown-20.jpg` shows
the complete “Ready... Set... Go!!!” message. `hill-font-fuel-watch.jpg` and
`hill-font-crash-check.jpg` show the full fuel-loss and crash messages. Fuel
loss and a 1,000-point crash both replayed through A; browser exit, fresh
launch and a second exit also passed. The separate host Asteroids ending
regression produced identical pixels with/without the new font registered.
See the game qualification ledger for remaining gameplay checks.
