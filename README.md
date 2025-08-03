# Scratch-3DS
A W.I.P. runtime made in C++ aimed to bring any Scratch 3 project over to the Nintendo 3DS and Wii U.

![Software running a simple Scratch Project](https://raw.githubusercontent.com/NateXS/Scratch-3DS/refs/heads/main/scratchcats3ds.gif)

## Controls
![Controls](https://raw.githubusercontent.com/NateXS/Scratch-3DS/refs/heads/main/scratch%203ds%20controls.png)
[3DS] Glide your finger/stylus on the bottom screen to move the mouse, and tap the bottom screen to click.
[Wii U] To use the mouse you must enter mouse mode by holding L. Use the D-pad to move the mouse, and press R to click.

### Other Plaforms

All controllers on all other platforms use the same control scheme.

## Unique Features

### 3DS Screen modes
- Any un-edited scratch project will be displayed on the Top Screen only.
- If using a modded Scratch client like Turbowarp, you can go into the projects' Advanced Settings and change the resolution.
- - Setting it to `400x480` will enable both the top and bottom screen.
- - Setting it to `320x240` will enable only the Bottom screen.
- - Setting it to `400x240` will make the project perfectly fit to the top screen.
- - NOTE: make sure to click `Store Settings In Project` on Turbowarp to properly apply the settings.

### Wii U Screen Modes
- Currently, projects display exactly the same on both the TV and the Gamepad, and there's no way to change Screen modes.

### Audio
- mp3, ogg, and wav audio formats are supported.
- A sound will take time to load if playing it for the first time.
- - Known Bug: On 3DS, if a sound has to load while other sounds are playing, all sounds will stop playing until the sound is loaded.
- If you play any sound from the "Stage," the sound will play as a "Streamed Sound," and will not need to load.
- Only one "Streamed Sound" can be playing at a time, so this is good for things like background music.
- Sounds may fail to load on 3DS if the file size of the song is too high, or if there's too many sounds loaded at once.

### Framerate
- When using a modded Scratch client like Turbowarp, you can enable the `60 FPS (Custom FPS)` advanced option, and change the FPS to any value.

### Differently Implemented Blocks
- The `Username` block returns the 3DS's nickname, and the Wii U's current Mii name.
- The `Touching __?` block uses simpler box collision, which may lead to projects working incorrectly.
- The `Stop 'All'` block brings you back to the Homebrew menu.

## Limitations
As this is in a very W.I.P state, you will encounter many bugs, crashes, and things that will just not work. 

**List of known limitations:**
- Images will only work if it's in .png, .jpg or .svg format
- If any vector image contains text, the text will not show up
- Extensions (eg: pen and music extensions) are not yet supported
- Some blocks may lead to crashing/unintended behavior (please open an issue if you know a block that's causing problems)
- [Wii U] The first controller connected will be the only one that will work
- [3DS] Performace is poor when lots of blocks are running at once
- [3DS] If you have a bunch of large images, some may not load
- [3DS] Some vector images will appear bigger than they should
- [3DS] Images cannot be over 1024x1024 in resolution
- [3DS] Some images may appear 'fuzzy' looking or have noticable inconsistencies


## Unimplimented blocks
- All say and think blocks
- All* Costume Effects
- - *`Ghost` Costume Effect is supported.
- Show/hide list
- When backdrop switches to
- When this sprite clicked
- When loudness > ___
- All Color touching blocks
- Set drag mode
- Loudness

## Roadmap / to-do list

### Runtime

- Bug fixing and Scratch Parity
- Get all blocks working
- Pen support
- Support most TurboWarp extensions
- Custom blocks to detect when being ran by Scratch 3DS

### 3DS

- Stereoscopic 3D support

### Wii U

- Dual screen support
- Improved controller support (multiple controllers, Wii Remotes)

### Other

- Ability to remap controls
- Download projects from the Scratch website
- Improve main menu UI

## Installation
There are 2 methods to install the runtime.
- Download the release or nightly build (easy)
- Build the file yourself (harder)

> [!NOTE]
> Nightly builds are significantly more unstable than releases and should probably only be used if you are instructed to do so in a bug report.

### Get up and running for 3DS

Download the .3dsx file in the Releases tab or [nightly build](https://nightly.link/NateXS/Scratch-3DS/workflows/nightly-3ds/main/Scratch%203DS%20Nightly.zip).

- Place the .3dsx file in the `3ds/` folder of your 3DS SD card, along with any Scratch projects you want to run.
> [!NOTE]
> Scratch 3DS is also on Universal Updater, so you can just download it there and keep it updated that way!

Then it should be as simple as opening the homebrew launcher on your 3DS and running the app!

### Get up and running for Wii U
Download the .zip file in the Releases tab or [nightly build](https://nightly.link/NateXS/Scratch-3DS/workflows/nightly-wiiu/main/Scratch%20Wii%20U%20Nightly.zip).

Unzip the file in your `sdcard:/wiiu/apps/` folder.

Place the scratch projects you want in `sdcard:/wiiu/scratch-wiiu/`
> [!NOTE]
> Scratch Wii U is also on the Homebrew App Store, so you can just download it there and keep it updated that way!

Then it should be as simple as opening the app on your Wii U!

### Building

In order to embed a Scratch project in the executable, you'll need to compile the source code.

If you would like to change the name of the app or any other information you can edit one of the Makefiles.
- For the 3DS you need to edit `Makefile_3ds` and change `APP_TITLE`, `APP_DESCRIPTION` and `APP_AUTHOR` to whatever you please.
- For the Wii U you need to edit `Makefile_wiiu` and change `APP_NAME`, `APP_SHORT_DESCRIPTION`, `APP_LONG_DESCRIPTION` and `APP_AUTHOR` to whatever you please.

#### Docker

The recommended way to compile Scratch 3DS is with Docker. To compile with Docker all you need installed is Docker and Buildx. To compile for the 3DS run `docker build -f Dockerfile.3ds --target exporter -o . .`. To compile for the Wii U run `docker build -f Dockerfile.wiiu --target exporter -o . .`

#### Manual

For 3DS and Wii U, you will need to have Devkitpro's SDKs, [Mist++](https://github.grady.link/mistpp), and a modified version of libcurl (instructions in mistpp-packages repo) installed.
- For the 3DS you will need the DevkitARM toolchain and libctru.
- - You will also need a 3DS compiled version of SDL2 and SDL2_mixer. See the [Nightly Build commands](https://github.com/NateXS/Scratch-3DS/blob/main/.github/workflows/nightly-3ds.yml) for a reference on how to compile SDL2 3DS for yourself.
- For the Wii U you will need the DevkitPPC toolchain, WUT, all SDL2-wiiu libraries, and libromfs-wiiu.

- Devkitpro's install instructions are available at : https://devkitpro.org/wiki/Getting_Started

Download the source code from the releases tab and unzip it.

Make a `romfs` folder inside the unzipped source code and put the Scratch project inside of that.
- The Scratch project MUST be named `project.sb3`, all lowercase.
- For faster load times/less limitations, you can also unzip the sb3 project file and put the contents into a new folder called `project`.

Then you need to compile the projects into proper Homebrew packages.
- For the 3DS you simply need to run `make`. Then copy the `Scratch-3DS.3dsx` file like you normally would.
- For the Wii U you need to run `make PLATFORM=wiiu all package` and then copy the `build/wiiu/scratch-wiiu` folder into the `sdcard:/wiiu/apps` folder on your sd card.

#### Compilation Flags

Compilation flags are used to select which features will be enabled in the compiled version of Scratch 3DS. To use a compilation flag simply add it to the end of the make command (e.g. `make ENABLE_BUBBLES=0`).

* 3DS - `ENABLE_BUBBLES` (default: `1`): If set to `1`, the loading screen is enabled, if set to `0` the screen is simply black during that time.
* `ENABLE_CLOUDVARS` (default: `0`): If set to `1`, cloud variable support is enabledd, if set to `0` cloud variables are treated like normal variables. If you're project doesn't use cloud variables it is recommended to leave this turned off. If you run into errors while building try turning this off and see if that fixes the errors.

## Other
This project is not affiliated with Scratch or the Scratch team.
