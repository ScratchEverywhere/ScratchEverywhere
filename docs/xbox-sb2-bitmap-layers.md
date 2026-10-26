# Scratch 2 bitmap text layers

Atari Breakout's `REMAKE`, x3/x2/x1 life counts and Game Over text are separate
bitmap assets referenced by `textLayerID`/`textLayerMD5`. The loader previously
read only `baseLayerID`, so the text disappeared and all three life costumes
shared the same base-image cache entry.

Scratch's [reference bitmap importer](https://github.com/scratchfoundation/scratch-vm/blob/develop/src/import/load-costume.js)
draws the text at (0,0) over the base, clips to the base canvas and then applies
resolution conversion. The image loader now follows that order with straight
RGBA source-over. It uses a distinct internal identity for each base/text pair;
ordinary base-only costumes retain their original identity. Both archive and
unpacked-file paths resolve the real layer filenames before decoding.

Xbox's disk texture precacher reads individual archive assets, so layered
costumes bypass that cache and use normal in-memory composition. Existing
base-only disk entries cannot replace a composite. Color-data restoration
retains the composite identity and uses the same decode path. SB3, UI images
and bitmap costumes with absent/negative text-layer IDs retain their old paths.

## Verification

`python3 tests/sb2_bitmap_layers_test.py` passed 10,000 deterministic RGBA blends
against a floating-point oracle, clipped/empty overlays and distinct key checks
with address/undefined sanitizers.

`python3 examples/make-sb2-bitmap-layers-test.py OUTPUT.sb2` creates four enlarged
bitmap samples: two overlays sharing a blue base, the unmodified base, and a
partially transparent base/overlay. In the actual Xbox executable under xemu,
all four had distinct image addresses and 8x8 dimensions. Twelve decoded RGBA
pixel probes matched exactly, including partial-alpha `(85,85,170,192)` and
clipping an oversized overlay. The framebuffer also showed the expected four
drawings while the frame counter advanced. Evidence is in ignored
`build-xbox/breakout-controller/text-layers/fixture{,-pixels,-stage}.*`.

Candidate `242d1c78e3760076202bc1999aa6e5873726a0499626a12c5b33c11be2f4f03c`
(9,691,136 bytes) then ran the original Breakout archive. The REMAKE subtitle
appeared; gameplay showed Score 20 and x1 instead of a blank life count. The
complete Game Over bitmap text appeared after the last loss. Runtime Green
Flag replay returned to the complete logo/subtitle. The saved gameplay and
ending captures are `breakout-layered-playing-x1.png` and `breakout-ending.png`.
The same fixture was then extracted to `D:/u` and selected with the browser’s
SB2 format hint supplied through GDB. `UnpackedInSD=true` was verified, the
frame counter advanced, all four images retained distinct buffers, and all
twelve RGBA probes matched through the file-loading path too. Evidence is
`unpacked-{fixture.png,pixels.json,stage.json,gdb.log}` in the same directory.
No executable or game script was patched for that selection. These emulator
runs use a GDB-only network initialization bypass. Physical-console retesting
is still pending. No retail
64MB, long-run audio or controller-usability pass is claimed by this fixture.
