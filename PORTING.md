# Porting

Porting Scratch Everywhere! can be either extremely easy or extremely hard, depending on what your target platform is.

To start, simply create a new Makefile in the `make/` directory, and add the target platform to the root `Makefile`.

## Platform That Support SDL2

If your target platform supports SDL2, you likely shouldn't need to change much. Firstly you'll likely need to configure some stuff in `source/scratch/os.cpp`, and if you're lucky, that should be it, if you're not well it should tell you the error!

## Platform That Don't Support SDL2

If your platform doesn't support SDL2 you have a lot more work, firstly create a new folder in the `source/` folder for your platform. Then create equivalent functions to the ones in `source/3ds` and `source/sdl`. You'll also need to add your platform in `source/scratch/os.cpp` just like with SDL2.

## Docker

We do require a `Dockerfile` be present for all ports to make building easier. If you don't know how to make one, we can make one for you, just ask.

## Creating a PR

Please have a checklist, it makes it much easier to track progress on ports.
