# Internal CMake Documentation

This is the internal documentation for our relatively complicated CMake setup.

## Dependencies

Dependencies are managed with [Catalog](https://github.com/catalog-cmake/catalog),
bootstrapped from `/cmake/cl-bootstrap.cmake` (vendored) and its shared recipe
registry at [catalog-cmake/repo](https://github.com/catalog-cmake/repo).
There's no more SE-specific `SE_DEPS` mode - Catalog's native stage pipeline
(`toolchain` -> `system` -> `package` -> `prebuilt` -> `source`) and its own
`CL_STAGES`/per-dependency override vars are the standard now; use those
directly (e.g. `-DCL_STAGES=source`, or Catalog's `VERSION`/`STATIC`/`OPTIONS`
keywords on a given `cl_add_dep` call) instead of anything SE-specific.

### Loading Dependencies

Dependencies are loaded with `cl_add_dep(target dependency_name)` (or
`cl_add_dep(dependency_name)` with no target for a dependency that only other
dependencies compose on top of, e.g. `libcurl` inside `mistpp`'s recipe). All
libraries are loaded to `deps::dependency_name`, which Catalog aliases
automatically from whichever real target a recipe stage produces.

Dependency **names must match the registry's `meta.cmake` casing exactly**
(e.g. `GLAD`, `GLFW`, `SDL2`, `mistpp` - see `~/catalog-repo/meta.cmake` or
wherever that repo is checked out) - CMake function names are case-insensitive
so a recipe's `_recipe_<name>_<stage>` functions still resolve either way, but
the registry lookup by package name is case-sensitive.

### Project-local recipe overrides

A handful of dependencies need SE-specific behavior the shared registry
recipe doesn't have (websocket-enabled cloud-vars libcurl, the
`SE_LUA_BACKEND` luajit/lua5.1 fallback and the `sol2` binding built on top of
it, libdlgmod's `SE_WINDOWING`-based event polling, and the vendored `ryuJS`
single-file dependency). These live in `/cmake/recipes/*.cmake` and are
registered with `cl_add_recipe(name path)` near the top of `CMakeLists.txt`,
which takes precedence over the shared registry's recipe of the same name.
Everything else - including SDL2/SDL3's audio-only build trimming, done with
plain `SOURCE_OPTIONS` on the `cl_add_dep` call instead of a recipe override -
resolves straight from the registry via `cl_repo()`.

## Backends

Backends are Renderers, Windowing Systems, and Audio Engines which all use the
same format in our CMake setup.

All backends should have an include guard like this:

```cmake
if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)
```

Then simply link dependencies to that interface.

Renderers should also set `SE_WINDOWING_VALID_OPTIONS` and
`SE_AUDIO_ENGINE_DEFAULT`.

## Platforms

Honestly, there are a lot of different variables platforms can set, I'd
recommend just looking at the existing configs.

## `CMakeLists.txt`

This is where we actually load the backends and link them to our main target.
This file also handles most of SE!'s build options like `SE_CLOUDVARS` and will
manage the dependencies related to those.
