# Debugging the Xbox guest with xemu and GDB

xemu's guest debugger lets us stop at source lines, inspect memory and registers,
and restart an emulated Xbox without resetting the physical console. It does
not replace the ten-game hardware qualification.

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
waiting in network initialization. Gameplay reproduction in xemu remains a
separate check from the verified debugger attachment.

References: [xemu guest debugging](https://xemu.app/docs/dev/debug/guest/),
[xemu command-line options](https://xemu.app/docs/cli/), and
[xemu networking](https://xemu.app/docs/networking/).
The alternative [xbdm_gdb_bridge](https://github.com/abaire/xbdm_gdb_bridge)
requires an XBDM-capable Xbox environment; that hardware route has not been set
up or tested here.
