"""Create four SB2 bitmap costumes: two overlays, base-only, and partial alpha.

All drawings are 8x8 base pixels, enlarged ten times. The first three share
one base asset; their top-left pixels must differ. The oversized first overlay
must be clipped to the base. A frame counter checks runtime liveness.
"""
import hashlib
import json
from pathlib import Path
import struct
import sys
import zipfile
import zlib

out = Path(sys.argv[1])

def png(width, height, pixels):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    rows = b"".join(b"\0" + pixels[y * width * 4:(y + 1) * width * 4] for y in range(height))
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")

def overlay(width, height, first, second):
    pixels = bytearray(width * height * 4)
    pixels[:4], pixels[4:8] = bytes(first), bytes(second)
    pixels[-4:] = bytes([255, 255, 0, 255])
    return png(width, height, pixels)

assets = {
    "0.svg": b'<svg xmlns="http://www.w3.org/2000/svg" width="480" height="360"><rect width="480" height="360" fill="#173351"/></svg>',
    "1.png": png(8, 8, bytes([0, 0, 255, 255]) * 64),
    "2.png": overlay(10, 10, [255, 0, 0, 255], [0, 255, 0, 128]),
    "3.png": overlay(8, 8, [0, 255, 0, 255], [255, 0, 0, 128]),
    "4.png": png(8, 8, bytes([255, 255, 0, 128]) * 64),
    "5.png": overlay(8, 8, [0, 0, 255, 128], [0, 0, 0, 0]),
}

def costume(base, text=None):
    name = str(base) + (".svg" if base == 0 else ".png")
    c = dict(costumeName="test", baseLayerID=base,
             baseLayerMD5=hashlib.md5(assets[name]).hexdigest() + Path(name).suffix,
             bitmapResolution=1, rotationCenterX=240 if base == 0 else 4,
             rotationCenterY=180 if base == 0 else 4)
    if text is not None:
        c.update(textLayerID=text, textLayerMD5=hashlib.md5(assets[f"{text}.png"]).hexdigest() + ".png")
    return c

sprites = [dict(objName=name, variables=[], lists=[], scripts=[], sounds=[],
                costumes=[costume(base, text)], currentCostumeIndex=0,
                visible=True, scratchX=x, scratchY=0, scale=10, direction=90,
                rotationStyle="normal")
           for name, base, text, x in [("Layer A", 1, 2, -180), ("Layer B", 1, 3, -60),
                                       ("Base only", 1, None, 60), ("Partial alpha", 4, 5, 180)]]
project = dict(objName="Stage", variables=[dict(name="Frames", value=0, isPersistent=False)],
    lists=[], scripts=[[10, 10, [["whenGreenFlag"], ["doForever", [
        ["changeVar:by:", "Frames", 1], ["wait:elapsed:from:", .1]]]]]],
    costumes=[costume(0)], currentCostumeIndex=0, sounds=[], tempoBPM=60,
    children=sprites + [dict(target="Stage", cmd="getVar:", param="Frames", mode=1,
                             visible=True, x=15, y=15, color=15629590)])
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as archive:
    archive.writestr("project.json", json.dumps(project))
    for name, data in assets.items():
        archive.writestr(name, data)
print(out)
