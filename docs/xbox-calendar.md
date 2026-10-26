# Xbox calendar reporters and Mario startup

Mario's first hardware launch retained the selected browser image and stopped
responding to pause and the controller reset combo. The same archive reproduced
the failure in an isolated xemu guest using matching XBE/ELF artifacts. Parsing,
texture precaching and the first runtime frame all completed. GDB found the
Intro thread at bytecode PC 137, `CurrentDateTime` with month selector 1; the
processor had faulted reading address `0x10` (`tm_mon` through a null pointer).

The pinned NXDK revision
`14d5ee97e73347c973f1f57b68b79ec08c9e77f2` implements `localtime()` as an
unconditional null return. All seven existing calendar reporters dereferenced
that return. Its `mktime()` is also a stub returning -1, so the previous
days-since-2000 epoch calculation was incorrect on Xbox.

The Xbox path now obtains calendar fields with `GetLocalTime`, which applies
the console timezone through NXDK's implemented WinAPI layer. Month and
day-of-week remain one-based in Scratch. Days since 2000 uses
`GetSystemTimeAsFileTime` and the UTC epoch at 2000-01-01. The
[Scratch VM implementation](https://github.com/scratchfoundation/scratch-vm/blob/develop/src/blocks/scratch3_sensing.js)
adds the starting timezone offset back to its elapsed-time calculation, making
that reporter equivalent to elapsed UTC days. The other platforms retain
their existing calendar and epoch paths.

## Verification

`python3 tests/xbox_time_test.py` compiles the actual `time.cpp` Xbox path with
a controlled SDK clock and address/undefined sanitizers. Eight cases cover the
epoch, a negative fractional day, leap day, the non-leap year 2100, milliseconds,
Sunday numbering, and console offsets that place the local and UTC dates on
different days. Calling either CRT stub aborts the test. All cases passed.
The non-Xbox source also passed a native syntax check.

`python3 examples/make-date-test.py OUTPUT.sb3` creates an eight-reporter
runtime fixture. It snapshots the local calendar and UTC day count at the
green flag; it is not an additional qualification game.

The embedded fixture passed in xemu using the real NXDK calendar APIs. GDB
read back year 2026, month 10, date 26, weekday 2 (Monday), 13:41:12, and UTC
days `9795.570286724536`. Independently converting that day count gives
`2026-10-26T13:41:12.773Z`, matching this guest's calendar within one second.
The snapshot, framebuffer, matching artifacts and verification JSON are in
ignored `build-xbox/calendar-fixture/`. The temporary embedded project was
removed and the regular browser build restored afterwards.

The regular browser debug build passed Mario's intro and rendered its first
level. Left/right movement, terrain scrolling, Up jumping, a question block
changing to its used state with a visible coin, Green Flag replay to the
starting area, and pause/exit to the normal browser were observed. GDB observed
month return value 10 during replay. A later apparently static pipe scene was
not a VM hang: frames continued and Left moved Mario away from the pipe.
The first pipe has not yet been crossed.

Evidence is in ignored `build-xbox/mario-calendar/`: matching XBE, ISO and ELF,
`gdb-session.log`, and native framebuffer captures `intro.png`,
`level-start.png`, `block-hit.png`, `replay.png`, `exit-browser.png`.
The debug XBE SHA-256 is
`5ad665bd087802619008774be360c5339a72a45a7566a7e0d32957d1ba600236`.

These xemu runs used a GDB-only return from `initNetworking` to avoid its
existing initialization stall; no network bypass is compiled into the fix.
The original user emulator and its mounted ISO were preserved. Builds used
`-DEXTRACT_XISO_BIN=OFF` and manually packaged distinct private ISOs rather
than overwriting the mounted `build-xbox/scratch-xbox.iso`.

Hardware verification of this fix is pending recovery to XBMC. The failed
physical launch's log is still to be retrieved. Mario still needs hardware
gameplay, first-level completion, loss/recovery, repeat launch, and audio
qualification; the emulator observations do not establish those passes.

The regular production browser XBE is ready at ignored
`build-xbox/mario-calendar-production/default.xbe` (9,678,848 bytes), SHA-256
`e255c72414aeda4c0b66b7c1303892ecf287a56e5b5179bfa303f0ac7da93beb`.
It contains neither the embedded fixture nor a network bypass. It has not yet
been installed on the physical Xbox. `SE_XBOX_DEBUG_SYMBOLS` was restored to
OFF for that production build; ISO packaging remains disabled in the local CMake cache while the user
emulator holds the original standard ISO open.
