# nxdk-gles11 evaluation

Andy Anderson recommended [Ryzee119/nxdk-gles11](https://github.com/Ryzee119/nxdk-gles11)
in [XboxDev sw-development](https://discord.com/channels/428359196719972353/428399682239332354/1564033860977561710)
on October 25, 2026, after Mike mentioned developing a raw NV2A renderer.

Run `bash scripts/build-xbox-gles11-probe.sh` after setting up the project's NXDK
toolchain with `scripts/build-xbox.sh`. The library is pinned to
`0cf17fa709f42158d11a1571f87705b692699bd3`, with its recursive dependencies,
under the ignored `.cache/toolchains` directory. Its gl4es wrapper is disabled.
The output is `build-xbox/gles11-probe/default.xbe`.

The probe draws a rotating four-color texture into a 256x256 framebuffer texture,
then samples that texture onto the 640x480 display. It logs framebuffer status
and frame count in `D:\gles11.log`, then returns to the dashboard after 45 seconds
of rendering. Initialization failures inside the library can still prevent that
deadline from being reached. It does not replace Scratch's launcher or settings.

## Initial hardware result

The probe built with our pinned NXDK and Clang 23.1.2, and rendered the rotating
four-color texture through the framebuffer on the 128MB Original Xbox.
Uploaded XBE SHA-256:
`eef37b29b37501b8344450e6a679dec9b9503bcca7ce5c035af6269d27d0a8ae`.
Installed separately in `F:\gles11-probe-20261025`.
Screenshot: ignored `build-xbox/hardware/gles11-probe-fbo.jpg`.
Retrieved log: framebuffer complete (`0x8cd5`), 2701 frames in 45011 ms,
and `GL_NO_ERROR` (`0x0`). Automatic return to XBMC was verified visually and
by retrieving the log over dashboard FTP. This tiny probe ran at about 60 fps;
that figure is not a benchmark of Scratch games or a comparison against SDL.

This establishes basic textured rendering and an offscreen framebuffer, not a
Scratch compatibility or performance pass. Retail 64MB hardware is untested.

## Integration decision

Use this library as a candidate foundation for an experimental renderer. Do not
replace the existing SDL Xbox renderer yet. At the pinned revision:

- `glReadPixels` in `gles_arrays_draw.c` only calls `glFinish`; it does not copy
  pixels to the caller.
- `glCopyTexImage2D` and `glCopyTexSubImage2D` in `gles_texture.c` are unimplemented.
- Our legacy OpenGL pen renderer calls `glCopyTexSubImage2D`, and its general
  `setRenderTarget` implementation is unsupported. Enabling gl4es alone would
  therefore not produce an equivalent Xbox backend.

A useful integration needs a framebuffer-based pen implementation, texture and
resource ownership, explicit video/context lifecycle, and validation of collision
and image pixel paths. Compare the same games and scenes against the SDL backend
before claiming any performance improvement. Keep SDL for controller/audio and
the working renderer until that compatibility work passes hardware tests.
