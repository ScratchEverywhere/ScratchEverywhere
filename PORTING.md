# Porting

Porting Scratch Everywhere! can be either extremely easy or extremely hard,
depending on what your target platform is.

## Build System

### Makefile (Deprecated)

> [!WARNING]
> Makefiles are not the primary build system for this repo, we **will** except
> them if they are strictly necessary for new ports but since we have finished
> migrating all of our current ports, you will have to use old commits as
> reference.

To start, simply create a new Makefile in the `make/` directory, and add the
target platform to the root `Makefile`.

### CMake

I'd personally recommend just searching for `NINTENDO_SWITCH` in the
`CMakeLists.txt` and seeing what you need to add.

## Platform That Support An Existing Renderer

If your target platform supports one of our existing renderers, you likely
shouldn't need to change much. Firstly you'll likely need to configure some
stuff in `source/scratch/os.cpp`, and if you're lucky, that should be it, if
you're not well it should tell you the error!

## Platform That Don't Support An Existing Renderer

If your platform doesn't support one of our existing renderers you have a lot
more work, firstly create a new folder in the `source/renderers` folder for the
renderer you're going to use. Then create equivalent functions to the ones in
`source/renderers/sdl2`. You'll also need to add your platform in
`source/scratch/os.cpp`.

## Docker

We do require a `Dockerfile` be present for all ports to make building easier.
If you don't know how to make one, we can likely make one for you, just ask.

## Creating a PR

Please have a checklist, it makes it much easier to track progress on ports.
