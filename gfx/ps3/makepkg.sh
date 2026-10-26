#!/bin/sh

APPID="NTXS00053"
CONTENTID="UP0001-"$APPID"_00-0000000000000000"

rm -Rf pkg
mkdir -p pkg/USRDIR/gfx

cp ICON0.PNG pkg
cp PIC1.PNG pkg
cp -r ../menu pkg/USRDIR/gfx


sprxlinker ../../build/scratch-ps3.elf
make_self_npdrm ../../build/scratch-ps3.elf pkg/USRDIR/EBOOT.BIN $CONTENTID

sfo.py --title "Scratch Everywhere!" --appid $APPID -f sfo.xml pkg/PARAM.SFO
pkg.py --contentid $CONTENTID pkg/ SE.pkg

rm -Rf pkg