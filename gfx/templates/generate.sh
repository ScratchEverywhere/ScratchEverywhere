#!/bin/bash
set -e

#  /\___/\
# | + w o |
#  ‾‾‾‾‾‾‾
# Scratch Everywhere! asset generation script

command -v magick >/dev/null || { echo "ImageMagick is required."; exit 1; }
command -v pngquant >/dev/null || { echo "pngquant is required."; exit 1; }

ITCHY_PATH="itchy.svg"
BG_PATH="background.png"
LOGO_PATH="../menu/logo.svg"
OUTLINED_LOGO_PATH="../menu/logo.svg"
GENERATION_PATH="generated"
mkdir -p "$GENERATION_PATH"
CENTER_COLOR=$(magick "$BG_PATH" -gravity center -crop 1x1+0+0 +repage -format "%[pixel:p{0,0}]" info:)

echo "[ASSETS] Generating..."
# GENERATION RECIPES
# --- Icon ---
echo "[ASSETS]-> Icon"
magick \
    "$BG_PATH" \
    -gravity center \
    -resize 1024x1024! \
    -extent 1024x1024 \
    -blur 0x95 \
    \( -density 600 -background none "$ITCHY_PATH" \
       -resize 860x860 \) \
    -gravity center \
    -composite \
    "$GENERATION_PATH/icon.png"

# --- Wide Icon ---
echo "[ASSETS]-> Wide Icon"
magick \
    -size 1280x720 xc:"$CENTER_COLOR" \
    \( -density 600 -background none "$LOGO_PATH" \
       -resize 1100x600 \) \
    -gravity center \
    -geometry +0+40 \
    -composite \
    "$GENERATION_PATH/wide-icon.png"

# --- Banner ---
echo "[ASSETS]-> Banner"
magick \
    "$BG_PATH" \
    -resize 684x256^ \
    -gravity center \
    -blur 0x50 \
    -extent 684x256 \
    \
    \( -density 600 -background none "$ITCHY_PATH" \
       -resize x320 \) \
    -gravity west \
    -geometry -175+0 \
    -composite \
    \
    \( -density 600 -background none "$OUTLINED_LOGO_PATH" \
       -resize x220 \
    \) \
    -gravity east \
    -geometry +40+25 \
    -composite \
    "$GENERATION_PATH/banner.png"

# --- BG with center logo ---
echo "[ASSETS]-> BG with center logo"
magick \
    "$BG_PATH" \
    -resize 1920x1080^ \
    -gravity center \
    -extent 1920x1080 \
    \
    \( -density 600 -background none "$OUTLINED_LOGO_PATH" \
       -resize 720x720 \
    \) \
    -gravity center \
    -geometry +0+30 \
    -composite \
    "$GENERATION_PATH/background-logo.png"

# --- BG with left logo ---
echo "[ASSETS]-> BG with left logo"
magick \
    "$BG_PATH" \
    -resize 1280x720^ \
    -gravity center \
    -extent 1280x720 \
    \
    \( -density 600 -background none "$OUTLINED_LOGO_PATH" \
       -resize 512x512 \
    \) \
    -gravity west \
    -geometry +60+0 \
    -composite \
    "$GENERATION_PATH/background-logo-left.png"

echo "[ASSETS] Done!"

# _-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_

# PLATFORM CONVERSIONS
echo "[PLATFORMS] Generating..."

mkdir -p \
    ../psp \
    ../vita/livearea/contents \
    ../ps4 \
    ../linux \
    ../wii \
    ../wiiu \
    ../3ds

# --- PSP ---
echo "[PLATFORMS]-> PSP"

magick "$GENERATION_PATH/wide-icon.png" \
    -resize 144x80! \
    ../psp/ICON0.png

magick "$BG_PATH" \
    -resize 480x272^ \
    -gravity center \
    -extent 480x272 \
    ../psp/PIC1.png


# --- Vita ---
echo "[PLATFORMS]-> Vita"

magick "$GENERATION_PATH/icon.png" \
    -resize 128x128! PNG32:/tmp/vita-icon.png
pngquant --posterize 1 --force --output ../vita/icon0.png /tmp/vita-icon.png
rm -f /tmp/vita-icon.png

magick "$GENERATION_PATH/background-logo-left.png" \
    -resize 840x500^ \
    -gravity center \
    -extent 840x500 \
    PNG32:/tmp/vita-bg.png
pngquant --posterize 1 --force --output ../vita/livearea/contents/bg.png /tmp/vita-bg.png
rm -f /tmp/vita-bg.png


# --- PS4 ---
echo "[PLATFORMS]-> PS4"

magick "$GENERATION_PATH/icon.png" \
    -resize 512x512! \
    ../ps4/icon0.png

cp -f "$BG_PATH" ../ps4/pic0.png

cp -f "$GENERATION_PATH/background-logo.png" ../ps4/pic1.png


# --- Linux ---
echo "[PLATFORMS]-> Linux"

cp -f "$ITCHY_PATH" ../linux/scratch-everywhere.svg


# --- Wii ---
echo "[PLATFORMS]-> Wii"

magick "$GENERATION_PATH/banner.png" \
    -resize 128x48! \
    -background "$CENTER_COLOR" \
    -alpha remove \
    -alpha off \
    PNG32:/tmp/wii-icon.png

pngquant --posterize 1 --force --output ../wii/icon.png /tmp/wii-icon.png
rm -f /tmp/wii-icon.png


# --- Wii U ---
echo "[PLATFORMS]-> Wii U"

magick "$BG_PATH" \
    -resize 854x480^ \
    -gravity center \
    -extent 854x480 \
    ../wiiu/drc-splash.png

magick "$GENERATION_PATH/background-logo.png" \
    -resize 1280x720^ \
    -gravity center \
    -extent 1280x720 \
    ../wiiu/tv-splash.png

magick "$GENERATION_PATH/icon.png" \
    -resize 128x128! \
    ../wiiu/icon.png

magick "$GENERATION_PATH/banner.png" \
    -resize 256x96! \
    ../wiiu/hbl-icon.png

magick "$GENERATION_PATH/icon.png" \
    -resize 256x256! \
    -quality 95 \
    ../wiiu/switch-icon.jpg


# --- 3DS ---
echo "[PLATFORMS]-> 3DS"

magick \
    -size 256x128 xc:none \
    \( -density 600 -background none "$LOGO_PATH" \
        -resize x115 \
    \) \
    -gravity center \
    -geometry +0+10 \
    -composite \
    -channel A \
    -threshold 50% \
    +channel \
    ../3ds/banner.png

magick "$GENERATION_PATH/icon.png" \
    -resize 48x48! \
    ../icon.png


echo "[PLATFORMS] Done!"
