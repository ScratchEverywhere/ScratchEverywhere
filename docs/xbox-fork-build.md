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
