# Internal CMake Documentation

This is the internal documentation for our relatively complicated CMake setup.

## Dependencies

The core part of our CMake setup is the dependencies. Dependency files reside in
`/cmake/deps/example_dep.cmake`. Our dependency loading system is designed for 3
modes: `fallback`, `source`, `system`; `source` forces dependencies to be built
from source, `system` forces dependencies to be present on the system, and
`fallback` will use system dependencies if possible and otherwise fallback to
building dependencies from source.

### Loading Dependencies

In order to load dependencies you use the `se_add_dependency` function present
in `/cmake/deps/add_dependency.cmake`. `se_add_dependency` either takes in 2 or
1 arguments. The 2 argument syntax takes in a target and a dependency name,
linking the dependency to the target. The 1 argument syntax simply loads a
library. The 1 argument syntax tends to be used for dependencies that rely on
other dependencies which can be seen in `/cmake/deps/mist++.cmake`. All
libraries are loaded to `deps::library_name` which is not guarenteed to be an
alias or not.

### Dependency Files

`se_add_dependency` will automatically detect any of the following targets as
the output library from a dependency file:

- `library_name`
- `library_name::library_name`
- `library_name::library_name-static`
- `PkgConfig::library_name`
- `deps::library_name`

If `deps::library_name` isn't set it will alias it to the above.

Dependency files are split into 3 phases. If a target is detected by
`se_add_dependency` during any of these phases it will stop searching.

The first phase is the global scope, this will be loaded regardless of what mode
SE! is loading dependencies in. The global scope is designed for platforms
defining specific versions of libraries that must be used in that way no matter
what mode we're building in. A good example of the global scope in use can be
found in `/cmake/deps/libcurl.cmake`.

The second phase will be run if SE! is building in `fallback` or `system` mode.
In this phase `se_add_dependency` will run the `_dep_system_library_name`
function and check for an outputed target.

The second phase will be run if SE! is building in `fallback` or `source` mode.
In this phase `se_add_dependency` will run the `_dep_source_library_name`
function and check for an outputed target.

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

The simply link dependencies to that interface.

Renderers should also set `SE_WINDOWING_VALID_OPTIONS` and
`SE_AUDIO_ENGINE_DEFAULT`.

## Platforms

Honestly, there are a lot of different variables platforms can set, I'd
recommend just looking at the existing configs.

## `CMakeLists.txt`

This is where we actually load the backends and link them to our main target.
This file also handles most of SE!'s build options like `SE_CLOUDVARS` and will
manage the dependencies related to those.
