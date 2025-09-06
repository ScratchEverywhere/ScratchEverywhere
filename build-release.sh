# A super simple script that builds all of the release binaries with CMake

# Vita
echo Building Vita...
rm -rf build-vita
cmake -B build-vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake -DCMAKE_BUILD_TYPE=Release -S .
cmake --build build-vita

# Wii
echo Building Wii...
rm -rf build-wii
$DEVKITPRO/portlibs/wii/bin/powerpc-eabi-cmake -B build-wii -DCMAKE_BUILD_TYPE=Release -S .
$DEVKITPRO/portlibs/wii/bin/powerpc-eabi-cmake --build build-wii

# Wii U
echo Building Wii U...
rm -rf build-wiiu
$DEVKITPRO/portlibs/wiiu/bin/powerpc-eabi-cmake -B build-wiiu -DCMAKE_BUILD_TYPE=Release -S .
$DEVKITPRO/portlibs/wiiu/bin/powerpc-eabi-cmake --build build-wiiu

# GameCube
echo Building GameCube...
rm -rf build-gamecube
$DEVKITPRO/portlibs/gamecube/bin/powerpc-eabi-cmake -B build-gamecube -DCMAKE_BUILD_TYPE=Release -S .
$DEVKITPRO/portlibs/gamecube/bin/powerpc-eabi-cmake --build build-gamecube

# 3DS
echo Building 3DS...
rm -rf build-3ds
$DEVKITPRO/portlibs/3ds/bin/arm-none-eabi-cmake -B build-3ds -DCMAKE_BUILD_TYPE=Release -S .
$DEVKITPRO/portlibs/3ds/bin/arm-none-eabi-cmake --build build-3ds

# Switch
echo Building Switch...
rm -rf build-nx
$DEVKITPRO/portlibs/switch/bin/aarch64-none-elf-cmake -B build-nx -DCMAKE_BUILD_TYPE=Release -S .
$DEVKITPRO/portlibs/switch/bin/aarch64-none-elf-cmake --build build-nx
