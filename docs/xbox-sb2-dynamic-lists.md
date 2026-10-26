# Computed Scratch 2 list selectors

Connect Four (aaronzoll) uses reporter expressions as list selectors. For
example, `join "column" (column)` selects its seven board lists, and
`join "columns" ""` selects the list of piece heights. The SB2 importer treated
all of these as literal fields, discarded the expressions, and emitted an
empty list ID. The first drop stamped a piece at the wrong height and did not
update the board. It then waited for the other player's input, giving the
appearance of an animation stuck midway through the board.

The original failure was observed in the Xbox runtime in xemu, with the
unchanged project selected from the normal browser. GDB found that all seven
board lists were empty after the drop, while the height list still had six
items. The compiled list slots included an empty name. Evidence is retained in
ignored `build-xbox/connect-four-dynamic/failure-before.json`,
`stage-before.json`, and `first-drop-before.png`.

The importer now preserves computed list selectors as inputs. List blocks
evaluate them at execution time, so changing a selector variable selects a
different list. Literal selectors retain their existing bytecode instructions;
computed selectors use the VM's existing legacy block bridge. No bytecode
instruction or cache format changes are required. ZIP projects are recompiled
on load; preserving the previously discarded expression blocks also changes
the parsed block counts used to validate unpacked-project cache entries.

Computed names resolve a sprite's local lists before the stage's lists.
The legacy literal lookup now follows that same scope order, matching the
VM's literal lookup. Missing computed names yield empty/zero results or ignore
mutations, rather than creating an unrelated empty-name list.

## Regression fixture

Generate an SB2 fixture with:

```sh
python3 examples/make-sb2-dynamic-list-test.py /tmp/Dynamic-Lists.sb2
```

It checks insertion, replacement, deletion, append, length, item lookup,
contents, contains, changing the selector, local shadowing, stage fallback,
missing names, and the unchanged literal path. The expected counters are
**PASSES=19, ERRORS=0**. Its final `bucket1` monitor should be visible and
`bucket2` hidden. This is a regression fixture, not a qualification game.

The real Xbox build passed all 19 checks in both execution engines in xemu.
The final native framebuffer captures were pixel-identical, including the
counter values and monitor visibility. Matching XBE/ISO/ELF, archived fixture,
GDB logs, stage snapshots and `verification.json` are in ignored
`build-xbox/connect-four-dynamic/fixture/`.

The embedded regression used the existing `Unzip::unpackedFormatHint` set to
SB2 through GDB, because embedded archives otherwise default to SB3. The legacy
run selected `useBytecode=false` before sprite parsing; the bytecode run used
the normal default. Both bypassed the existing network initialization stall
through GDB. These are test harness settings, not compiled production changes.
An initial attempt to override the format by returning from its optimized
function unwound incorrectly; that attempt was reset and is excluded from
the passing results.

## Actual Connect Four project

The regular browser build selected and loaded the unchanged SB2 archive with
normal format detection. Red's D-pad drop reached the bottom row, and the
right-stick WASD profile moved and dropped the other player's piece. Subsequent
drops stacked at the proper heights. A vertical red four-in-a-row produced the
authored win backdrop. Evidence: ignored `vertical-win.png` and
`browser/gdb-session.log` under `build-xbox/connect-four-dynamic/`.

The source project also initializes `space` to 0, while its turn/costume logic
checks for the string `unpressed`. As a result, player remains 0 and both colors
look red until Space toggles the mode. Toggling twice restored the red/yellow
costumes and keyboard mode in this test. Its visible mode slider also overlaps
the win text. These authored-project issues remain to be addressed separately;
the runtime is not changing number/string comparisons to accommodate them.

The regular browser XBE SHA-256 is
`2f877e8c380bd1696f8facb0e2f51a0208ef02da26204b4425a8849b6614ebfb`
(9,682,944 bytes), with matching symbols retained in ignored `browser/`.
This build also contains the calendar reporter repair. Debug symbols remain
enabled for ongoing emulator investigation; the XBE's debug sections are
stripped by the normal packaging step, with symbols saved separately.

Actual-game and physical Xbox qualification remain separate from the fixture.
The physical Xbox still needs recovery to XBMC before the new build can be
installed and its failed launch log retrieved. Replay, additional win
directions, full-column handling and audio have not yet been qualified.
