# Debugging the Xbox guest with xemu and GDB

xemu's guest debugger lets us stop at source lines, inspect memory and registers,
and restart an emulated Xbox without resetting the physical console. It does
not replace the twenty-game hardware qualification.

## Build the symbol companion

On Apple Silicon, the tested tools are Homebrew's `i386-elf-gdb` and `binutils`:

```sh
brew install i386-elf-gdb binutils
bash scripts/build-xbox.sh -DSE_XBOX_FORCE_480P=ON -DSE_MENU=ON \
  -DSE_LOADSCREEN=OFF -DSE_USE_LWMEM=OFF -DSE_XBOX_DEBUG_SYMBOLS=ON
```

`SE_XBOX_DEBUG_SYMBOLS` defaults to OFF. When enabled, application sources keep
their Release optimization and gain DWARF 4 information. The linker retains
dead-code removal and identical-code folding. GNU objcopy converts the linked
PE image to `build-xbox/scratch-xbox-symbols.elf`, preserving virtual addresses.
Debug sections are then stripped from the PE before CXBE packages it. Keeping
those sections in the XBE made it about 107MB; stripping them restored a roughly
9MB XBE while leaving the approximately 110MB symbol companion on the host.
Always pair an ISO/XBE with the symbols from the same build.

## Launch and attach

Use Python 3.11 or newer. Supply an existing working xemu TOML config containing
your boot ROM, BIOS, HDD and EEPROM paths; no firmware is downloaded by the
helper. For the tested Mac installation:

```sh
/opt/homebrew/bin/python3.13 scripts/debug-xemu.py \
  --config "$HOME/Library/Application Support/xemu/xemu/xemu.toml"
```

The helper copies the HDD and EEPROM into ignored `build-xbox/xemu-debug` on
first use, writes a separate `guest.toml`, enables 128MB and a NAT/DHCP backend,
and starts paused. HDD writes are temporary (`-snapshot`). It preserves the
source configuration and source disk files. Stop this debug instance before
launching the helper again or rebuilding/replacing its mounted ISO. Its GDB
endpoint is explicitly bound to `127.0.0.1:1234`, equivalent to `-s` with a local
bind address. `--memory 64` permits a separate retail-memory test.

For controller-driven tests, add `--keyboard-controller`. This binds player
one in the private guest configuration to xemu's keyboard gamepad and disables
automatic physical-controller binding for that guest. The tested mapping uses
A/B/X/Y for face buttons, arrows for the D-pad, Return for Start, Backspace for
Back, and 1/2 for White/Black. Keyboard input through the isolated test window
successfully activated the application's normal menu.

Add `--persist-disk` when installing projects into the private guest HDD; this
omits `-snapshot` so those copied-disk writes survive later test runs. The
source HDD and EEPROM are still preserved. The helper rejects a work directory
whose generated config, private HDD or EEPROM aliases any source input,
including existing hard links or symlinks. The default remains temporary HDD writes.
Only run one instance against a given private work directory.

In another terminal, from this repository:

```sh
i386-elf-gdb -q -nx build-xbox/scratch-xbox-symbols.elf \
  -x scripts/xbox-guest.gdb
```

The command file uses a hardware breakpoint at `main`, then continues boot.
A software breakpoint set before boot would be overwritten when the BIOS
loads the XBE. Once stopped, useful commands include:

```gdb
bt
info registers eip esp ebp
x/12i $eip
hbreak source/runtime/blocks/control.cpp:64
continue
xbox-clones
monitor system_reset
```

Ctrl-C interrupts guest execution. `continue` resumes it; `detach` resumes and
disconnects. `monitor system_reset` resets the emulated Xbox. Hardware
breakpoints can survive that reset. Add `--qmp /private/tmp/se-xemu-debug.qmp`
to the launcher when automated reset/status access is useful.

## Verified on October 25, 2026

With xemu 0.8.136 and i386-elf-gdb 17.2, the real application ISO stopped in
`main()` at `source/main.cpp:234`, EIP `0x6859b`. The address matched its source
and disassembly. After an emulated reset, another breakpoint stopped inside
`initNetworking`; GDB stepped out of the inlined `isOnline` function and showed
six application frames through `OS::init`, `Scratch::initializeRuntime`,
`initApp` and `main`. Reading the runtime counters returned 0 clones and a 300
clone limit before game loading. Local proof logs are in ignored
`build-xbox/xemu-debug/gdb-source-proof.log`.

Limitations: NXDK uses Microsoft C++ symbol names, which this GDB does not always
resolve by qualified C++ name. Source-file/line breakpoints work. The
`xbox-clones` helper uses explicit mangled symbols and casts to read the counters.
Release optimization can hide locals. The system-level stub exposes the CPU,
not an Xbox thread-aware debugger; interrupting in the kernel idle loop did not
produce an application backtrace. Set an application breakpoint instead.
NAT was necessary here: the disconnected default network hub left startup
waiting in network initialization. Subsequent Appel work caught an additional
startup abort in malformed Settings.json and added a default-settings fallback.
See [the Appel clone investigation](appel-clone-debugging.md) for the first
script-level use of this workflow.

Do not infer guest-usable RAM from `--memory 128` alone. This BIOS reported
only 64MB through `MmQueryStatistics`, although QMP confirmed 128MB of emulated
RAM. Appel's GDB diagnostic omitted audio payloads while retaining byte-identical
project JSON; the full game is tested separately on the actual 128MB Xbox.
Release DWARF also reported a misleading value for one optimized Boolean local;
checking the actual compare instruction and its stack byte resolved it.

In the October 26 Frogger run on this Apple Silicon/xemu 0.8.136 setup, data
watchpoints stalled the private guest and its QMP connection. A host process
sample repeatedly showed `cpu_check_watchpoint`, `tb_check_watchpoint` and
translation-block invalidation. Restarting that isolated guest without data
watchpoints restored progress. Software breakpoints at input polls plus
read-only state snapshots completed the scoring/replay probes. This is a
debugger limitation observed in that setup, not evidence of a Frogger freeze;
code breakpoints at startup still worked. The sample and logs are retained in
ignored `build-xbox/xemu-frogger-score/`.

References: [xemu guest debugging](https://xemu.app/docs/dev/debug/guest/),
[xemu command-line options](https://xemu.app/docs/cli/), and
[xemu networking](https://xemu.app/docs/networking/).
The alternative [xbdm_gdb_bridge](https://github.com/abaire/xbdm_gdb_bridge)
requires an XBDM-capable Xbox environment; that hardware route has not been set
up or tested here.

For the separate intermittent physical-console startup rendering fault, see
[the GPU startup investigation](xbox-startup-rendering.md). Its same-boot pixel
experiments isolated the fixed-function vertex path, leading to an explicit
screen-space vertex program and repeated production hardware launches.


## Controller-driven private guest (October 26)

The helper's opt-in keyboard binding was exercised through xemu's emulated Xbox
controller, with normal browser selection and unmodified project archives.
Twenty archives plus the installed hardware profiles/settings were uploaded
into the private guest's `E:\ScratchEverywhere` using the official
[xemu dashboard](https://github.com/xemu-project/xemu-dashboard) FTP server and
complete SHA-256 readback verification. The ordinary browser listed the games.

Sokoban's first two puzzles completed through normal controller input, followed
by a successful runtime pause and Exit Project back to the browser. Puzzle-one
input was Up, Down, Left; puzzle two was Up, Down, Down, Up, Left, Right, Right.
Inputs were separated to respect the project's authored movement cooldown.
Evidence is in ignored `build-xbox/xemu-debug/controller-guest-installed.json`,
`controller-twenty-browser.png`, `sokoban-level1-complete.png`,
`sokoban-level2-complete.png` and `sokoban-browser-return.png`. The completion
images record the subsequent puzzle/level counters after transition.
These are emulator checks, not physical Xbox qualification.

The launcher also passed a temporary fixture check for default snapshot mode,
opt-in keyboard/persistent mode, unchanged source bytes, and rejection of HDD/EEPROM hard links, a cross-file EEPROM-to-HDD symlink,
and a generated-config hard link to the source configuration. Python 3.13 syntax compilation passed.
A complete process quit/relaunch with `--persist-disk` retained the installed
games and returned to the populated browser
(`controller-persisted-browser.png`).
