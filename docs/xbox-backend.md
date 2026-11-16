# Original Xbox backend

The `xbox` renderer and windowing backend use the NV2A GPU directly through
pbkit. The former SDL3-based Xbox renderer has been removed.

Use these CMake selections for a dedicated Xbox build:

```text
SE_RENDERER=xbox
SE_WINDOWING=xbox
SE_AUDIO_ENGINE=xbox
```

The default `xbox` audio engine uses nxaudio. It is independent of the renderer
rename.

Existing Xbox CMake caches selecting `nv2a` rendering or `nv2a`/`sdl3` windowing
migrate to `xbox`. Saved renderer/windowing names `NV2A` and legacy `SDL3`
migrate to `Xbox` when using the Xbox renderer. The canonical command-line
option is `--xbox`; `--nv2a` remains a compatibility alias.

The existing NV2A texture-cache directory names and formats are preserved.
Compatible cache files from the removed renderer remain readable through a
naming-only compatibility helper, without linking its implementation.

GPU-specific diagnostic flags keep their `NV2A` names.
`SE_XBOX_NV2A_HYBRID_HUE`, `SE_XBOX_NV2A_BENCHMARK`, and
`SE_XBOX_NV2A_RESIDENCY_EXPERIMENT` remain OFF by default.