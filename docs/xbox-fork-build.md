# Building this fork for the Original Xbox

This fork starts from `ScratchEverywhere/ScratchEverywhere` at
`79a843c33be1f290450bf6c98fc0a7616a850a76`. MikeDev101's original
`ScratchEverywhere-OGXbox` fork was archived on October 19, 2026; its README
directs users to upstream, which now includes the Xbox port.

The Xbox target uses NXDK, the Ryzee119 SDL3 port, and the compatibility
patches in `cmake/patches`. The SDK revision matches `docker/Dockerfile.xbox`.

## Native build

Install Git, CMake, Make, LLVM (including LLD), Bison, Flex and Rust/Cargo.
Cargo builds the `dsp56300-asm` host tool used by Xbox audio. On macOS use
Homebrew LLVM rather than Apple's compiler.

If Rust is not already installed, an isolated official rustup installation can
be created without changing your shell profile:

```sh
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs -o /tmp/se-rustup-init.sh
CARGO_HOME="$PWD/.cache/toolchains/cargo" \
RUSTUP_HOME="$PWD/.cache/toolchains/rustup" \
  sh /tmp/se-rustup-init.sh -y --profile minimal --default-toolchain stable --no-modify-path
```

The helper automatically uses that local installation. Then build:

```sh
bash scripts/build-xbox.sh
```

The script keeps its patched SDK under `.cache/toolchains/nxdk` and builds
under `build-xbox`. It does not use or patch a shared NXDK installation.
The first build downloads dependencies and compiles NXDK. To limit application
compilation parallelism, set `BUILD_JOBS` (default: 4). Extra arguments are passed
to CMake, for example `bash scripts/build-xbox.sh -DSE_DECTALK=OFF`.

Expected outputs are `build-xbox/default.xbe` for a modded Xbox and
`build-xbox/scratch-xbox.iso` for xemu. Producing these files does not establish
that a build boots or works on hardware; test separately.

### First successful local build

On October 25, 2026, `bash scripts/build-xbox.sh` completed on macOS with
Homebrew Clang 23.1.2 and Rust 1.99.1, using the default Xbox options. The XBE
header begins with `XBEH`; packaging added that XBE to the XISO successfully.
No emulator or hardware boot test has been performed.

| Output | Bytes | SHA-256 |
| --- | ---: | --- |
| `default.xbe` | 9437184 | `f983cf5b0d3f77a4129a45a9f5a7aaa1ee72bcd910ac6ab3438bc374f7dceb36` |
| `scratch-xbox.iso` | 10027008 | `141ac60ccff104b5422f95714be05de96e90f4f4f6dce0973f4d9708a00ab32c` |

## Upstream container build

With Docker running, use the existing upstream image definition:

```sh
mkdir -p romfs build-xbox-export
docker build --file docker/Dockerfile.xbox --target exporter \
  --output type=local,dest=build-xbox-export .
```

The Xbox workflow is `.github/workflows/nightly-xbox.yml` and supports manual
dispatch. Forks may require enabling GitHub Actions first.

## Running projects

Put Scratch projects in `E:\ScratchEverywhere` on the console or emulated HDD.
See <https://scratcheverywhere.github.io/install> for current installation details.

## Expanded-memory hardware tests

Xbox packaging clears the XBE's 64MB limit by default
(`SE_XBOX_ALLOW_EXPANDED_RAM=ON`). This permits an expanded console to use its
additional RAM; it does not add RAM to a retail console. Python 3 is required
for this packaging step. Set the option to `OFF` to retain CXBE's original
memory-limit flag for comparison.

The 128MB hardware test loaded Appel with complete map lists using NXDK's
native allocator. Reproduce that configuration with:

```sh
bash scripts/build-xbox.sh -DSE_XBOX_FORCE_480P=ON -DSE_MENU=ON \
  -DSE_LOADSCREEN=OFF -DSE_USE_LWMEM=OFF
```

The default lwmem configuration remains available and needs a separate Appel
loading investigation. For diagnostics only, `SE_XBOX_TRACE_GAME_STATE=ON`
logs stage variables when the runtime pause menu opens. It defaults to `OFF`.
See `xbox-game-qualification.md` for the test limitations and outstanding bugs.


## Upstream synchronization (October 26, 2026)

Merged upstream `main` through
[`ac8a468b`](https://github.com/ScratchEverywhere/ScratchEverywhere/commit/ac8a468bb42681127bd3a0ba1507a1d7f0f4390d),
including the new bytecode VM (`4cb9003d`) and libdlgmod update (`94b040fb`).
The pre-merge fork tip is retained at
`backup/pre-upstream-20261026-8a1efc0f`.

The upstream ROMFS repair supersedes the fork's overlapping embedded-archive
workaround: embedded projects always retain their archive in memory, even when
`sb3InRam=false`; only external archives are subject to the 6/24MiB RAM budget.
The failed-file-open fallback now copies the caller's stream into owned memory
rather than retaining a stream the caller subsequently destroys. The fork
keeps complete authored Xbox map lists instead of truncating them.

Upstream split the runtime into smaller files. The empty-input defaults were
carried into `runtime_inputs.cpp`, preserving false/zero/empty results in legacy
execution and VM fallbacks. Xbox's physical Start hold still opens pause after
three seconds of wall time in `runtime_loop.cpp`. Upstream's bytecode VM remains
enabled by default; it was not disabled to resolve the merge. SB2 costume name
lookup, per-project controller profiles, multi-controller/reset/replay behavior,
browser confirmation-release gating and the GPU/color/pen fixes are retained.

Validation: the NXDK regular and embedded builds passed, as did all 1,756 host
color-effect cases. An embedded fixture explicitly set `sb3InRam=false` and
`useBytecode=true`. GDB verified archive retention was true and stopped in the
new bytecode executor. The fixture rendered its SVG after loader completion and
reported PASSES=3, ERRORS=0, EMPTY ADD=7. A second boot selected the legacy engine
under GDB before sprite parsing, stopped in its executor, and produced a
pixel-identical passing image. Networking was skipped under GDB only for these
two diagnostic runs. Evidence is in ignored `build-xbox/upstream-sync`, with
matching diagnostic ISO/XBE/ELF files and settings retained under `fixture/`.

The final normal build restores menu mode, has no embedded fixture, and disables
debug symbols and renderer/game tracing. Its XBE is 9,633,792 bytes, SHA-256
`f90ec7b0ba1aaa460dcbaa8cfbdce69945ae878eed422f97042d72852dab918f`.
It reached the populated browser in xemu without debugger overrides. At sync completion this build had not yet been tested on the physical Xbox.
It was subsequently installed in a separate test folder for Appel and Asteroids
checks; the regular Games launcher retains the previous production build.
See `xbox-game-qualification.md` for that testing and the later SB2 text repair.
Qualification across all twenty games remains incomplete.
The normal-build emulator smoke test also passed browser selection, SB2
named-costume rendering, the installed D-pad profile (a crate push), physical
Start-hold pause, and Exit Project back to the browser with an 800ms confirm.
Images are `normal-browser.png`, `normal-sb2-moved.png`, `normal-pause.png` and
`normal-browser-return.png`; `validation.json` records artifact hashes.
