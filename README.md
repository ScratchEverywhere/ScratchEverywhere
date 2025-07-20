# Scratch-3DS
A W.I.P. runtime made in C++ aimed to bring any Scratch 3 project over to the Nintendo 3DS.

![Software running a simple Scratch Project](https://raw.githubusercontent.com/NateXS/Scratch-3DS/refs/heads/main/scratchcats3ds.gif)

## Controls
![Controls](https://raw.githubusercontent.com/NateXS/Scratch-3DS/refs/heads/main/scratch%203ds%20controls.png)
To use the mouse you must enter mouse mode by holding L. Use the D-pad to move the mouse, and press R to click.

### Other Plaforms

All controllers on all other platforms use the same control scheme.

## Limitations
As this is in a very W.I.P state, you will encounter many bugs, crashes, and things that will just not work. 

**List of known limitations:**
- Sound is not yet implemented
- Performance on old 3DS starts to tank with many blocks running
- There is no vector/svg sprite rendering. Images will only render if converted to bitmap beforehand, otherwise the sprite will show as a black square
- Images will only work if it's in .png or .jpg format
- If you have a bunch of large images, some may not load
- Images cannot be over 1024x1024 in resolution
- Extensions (eg: pen and music extensions) are not yet supported
- Some blocks may lead to crashing/unintended behavior (please open an issue if you know a block that's causing problems)


## Unimplimented blocks
- All say and think blocks
- All Costume Effects
- Cloud variables
- Show/hide variable | Show/hide list
- When backdrop switches to
- When this sprite clicked
- When loudness > ___
- All Color touching blocks
- Set drag mode
- Loudness

## Roadmap / to-do list
- Pen support (Highly Requested!)
- Better start menu
- File picker for Scratch projects
- Ability to remap controls
- Get all blocks working
- Audio support
- Turbowarp extensions
- Cloud variables (maybe.....)

## Installation
There are 2 methods to install the runtime.
- Download the release (easy)
- Build the file yourself (harder)

### Get up and running the easy way
Download the .3dsx file in the Releases tab.

Place the .3dsx file in the `3ds/` folder of your 3DS SD card, along with any Scratch projects you want to run.
- For Beta 4 and below: The Scratch project MUST be named `project.sb3`, all lowercase.

Then it should be as simple as opening the homebrew launcher on your 3DS and running the app.


### Building

In order to embed a Scratch project in the Scratch 3DS executable, you'll need to compile the source code.

No matter the platfrom, you will need to have Devkitpro's SDKs installed. For the 3DS you will need the DevkitARM toolchain and libctru, and for the Wii U you will need the DevkitPPC toolchain, WUT, and all SDL2 libraries for the Wii U.

- Devkitpro's install instructions are available at : https://devkitpro.org/wiki/Getting_Started

Download the source code from the releases tab and unzip it.

Make a `romfs` folder inside the unzipped source code and put the Scratch project inside of that.
- The Scratch project MUST be named `project.sb3`, all lowercase.
- For faster load times/less limitations, you can also unzip the sb3 project file and put the contents into a new folder called `project`.

If you would like to change the name of the app or any other information you can edit one of the Makefiles. For the 3DS you need to edit `Makefile_3ds` and change `APP_TITLE`, `APP_DESCRIPTION` and `APP_AUTHOR` to whatever you please. For the Wii U you need to edit `Makefile_wiiu` and change `APP_NAME`, `APP_SHORT_DESCRIPTION`, `APP_LONG_DESCRIPTION` and `APP_AUTHOR` to whatever you please.

Then it should be as simple as running `make` in the source code folder.

## Other info
This project is not affiliated with Scratch or the Scratch team.
